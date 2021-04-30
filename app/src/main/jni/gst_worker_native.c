//
// Created by puter on 2020-08-06.
//

#include "gst_worker_native.h"

#include <android/native_window.h>
#include <android/native_window_jni.h>
#include <gst/video/video.h>


jstring gst_version_retreieve()
{
    return gst_version_string();
}

GST_DEBUG_CATEGORY_STATIC (debug_category);
#define GST_CAT_DEFAULT debug_category

/*
 * These macros provide a way to store the native pointer to CustomData, which might be 32 or 64 bits, into
 * a jlong, which is always 64 bits, without warnings.
 */
#if GLIB_SIZEOF_VOID_P == 8
#define GET_CUSTOM_DATA(env, thiz, fieldID) (CustomData *)(*env)->GetLongField (env, thiz, fieldID)
#define SET_CUSTOM_DATA(env, thiz, fieldID, data) (*env)->SetLongField (env, thiz, fieldID, (jlong)data)
#else
#define GET_CUSTOM_DATA(env, thiz, fieldID) (CustomData *)(jint)(*env)->GetLongField (env, thiz, fieldID)
#define SET_CUSTOM_DATA(env, thiz, fieldID, data) (*env)->SetLongField (env, thiz, fieldID, (jlong)(jint)data)
#endif

/* Structure to contain all our information, so we can pass it to callbacks */
typedef struct _CustomData {
    jobject app;           /* Application instance, used to call its methods. A global reference is kept. */
    GstElement *pipeline;  /* The running pipeline */
    GMainContext *context; /* GLib context used to run the main loop */
    GMainLoop *main_loop;  /* GLib main loop */
    gboolean initialized;  /* To avoid informing the UI multiple times about the initialization */
    GstElement *video_sink; /* The video sink element which receives VideoOverlay commands */
    ANativeWindow *native_window; /* The Android native window where video will be rendered */
} CustomData;

/* These global variables cache values which are not changing during execution */
extern pthread_t gst_app_thread;
extern pthread_key_t current_jni_env;
extern JavaVM *java_vm;
jfieldID custom_data_field_id;
//extern jmethodID set_message_method_id;
jmethodID on_gstreamer_initialized_method_id;

char this_session_pipeline[512];



jboolean gst_native_class_init(JNIEnv *env, jclass clazz)
{
    custom_data_field_id = (*env)->GetFieldID(env, clazz, "native_custom_data", "J");
//    set_message_method_id = (*env)->GetMethodID(env, clazz, "setMessage", "(Ljava/lang/String;)V");
    on_gstreamer_initialized_method_id = (*env)->GetMethodID(env, clazz, "onGStreamerInitialized", "()V");

    if(!custom_data_field_id || !on_gstreamer_initialized_method_id) {
        __android_log_print(ANDROID_LOG_ERROR, "MYTAG", "The calling class does not implement all necessary interface methods");
        return JNI_FALSE;
    }

    return JNI_TRUE;
}



/* Register this thread with the VM */
static JNIEnv *attach_current_thread (void) {
    JNIEnv *env;
    JavaVMAttachArgs args;

    LOGD ("Attaching thread %p", g_thread_self ());
    args.version = JNI_VERSION_1_4;
    args.name = NULL;
    args.group = NULL;

    if ((*java_vm)->AttachCurrentThread (java_vm, &env, &args) < 0) {
        GST_ERROR ("Failed to attach current thread");
        return NULL;
    }

    return env;
}

/* Unregister this thread from the VM */
void detach_current_thread (void *env)
{
    LOGD ("Detaching thread %p", g_thread_self ());
    (*java_vm)->DetachCurrentThread (java_vm);
}

/* Retrieve the JNI environment for this thread */
static JNIEnv *get_jni_env (void) {
    JNIEnv *env;

    if ((env = pthread_getspecific (current_jni_env)) == NULL) {
        env = attach_current_thread ();
        pthread_setspecific (current_jni_env, env);
    }

    return env;
}

/* Check if all conditions are met to report GStreamer as initialized.
 * These conditions will change depending on the application */
static void check_initialization_complete (CustomData *data) {
    JNIEnv *env = get_jni_env ();
    if (!data->initialized && data->native_window && data->main_loop) {
        LOGD ("Initialization complete, notifying application. main_loop:%p", data->main_loop);

        gst_video_overlay_set_window_handle(GST_VIDEO_OVERLAY(data->video_sink), (guintptr)data->native_window);

        (*env)->CallVoidMethod (env, data->app, on_gstreamer_initialized_method_id);
        if ((*env)->ExceptionCheck (env)) {
            GST_ERROR ("Failed to call Java method");
            (*env)->ExceptionClear (env);
        }
        data->initialized = TRUE;
    }
}




/* Retrieve errors from the bus and show them on the UI */
static void error_cb (GstBus *bus, GstMessage *msg, CustomData *data) {
    GError *err;
    gchar *debug_info;
    gchar *message_string;

    gst_message_parse_error (msg, &err, &debug_info);
    message_string = g_strdup_printf ("Error received from element %s: %s", GST_OBJECT_NAME (msg->src), err->message);
    g_clear_error (&err);
    g_free (debug_info);
    g_free (message_string);
    gst_element_set_state (data->pipeline, GST_STATE_NULL);
}

/* Notify UI about pipeline state changes */
static void state_changed_cb (GstBus *bus, GstMessage *msg, CustomData *data) {
    GstState old_state, new_state, pending_state;
    gst_message_parse_state_changed (msg, &old_state, &new_state, &pending_state);
    if (GST_MESSAGE_SRC (msg) == GST_OBJECT (data->pipeline)) {
        gchar *message = g_strdup_printf("State changed to %s", gst_element_state_get_name(new_state));
        g_free (message);
    }
}

static void *app_function (void *userdata) {
    JavaVMAttachArgs args;
    GstBus *bus;
    CustomData *data = (CustomData*) userdata;
    GSource *bus_source;
    GError *error = NULL;

    LOGD("Create pipeline in CustomData at %p", data);

    data->context = g_main_context_new();
    g_main_context_push_thread_default(data->context);

//    data->pipeline = gst_parse_launch("videotestsrc pattern=18 ! tee name=t "
//                                      "t. ! queue ! jpegenc ! rtpjpegpay ! udpsink host=192.168.137.1 port=9000 "
//                                      "t. ! queue ! autovideosink "
//                                      , &error);
//    data->pipeline = gst_parse_launch("udpsrc port=9000 ! application/x-rtp,encoding-name=JPEG,payload=26 ! rtpjpegdepay ! jpegdec ! autovideosink", &error);

//    data->pipeline = gst_parse_launch("videotestsrc pattern=18 ! tee name=t "
//                                      "t. ! queue ! openh264enc ! rtph264pay ! udpsink host=192.168.137.207 port=9000 "
//                                      "t. ! queue ! autovideosink "
//                                      , &error);
//    data->pipeline = gst_parse_launch("udpsrc port=9000 ! application/x-rtp,payload=127 ! rtph264depay ! h264parse ! openh264dec ! autovideosink", &error);


    data->pipeline = gst_parse_launch(this_session_pipeline, &error);

    if(error) {
        gchar *message = g_strdup_printf("Unable to build pipeline : %s", error->message);
        LOGE("%s",message);
        g_clear_error(&error);
        LOGE("pipeline build error - exiting...");
        g_free(message);
        return NULL;
    }

    gst_element_set_state(data->pipeline, GST_STATE_READY);
    data->video_sink = gst_bin_get_by_interface(GST_BIN(data->pipeline), GST_TYPE_VIDEO_OVERLAY);
    if(!data->video_sink) {
        GST_ERROR ("Could not retrieve video sink");
        return NULL;
    }

    bus = gst_element_get_bus(data->pipeline);
    bus_source = gst_bus_create_watch(bus);
    g_source_set_callback(bus_source, (GSourceFunc)gst_bus_async_signal_func, NULL,NULL);
    g_source_attach(bus_source,data->context);
    g_source_unref(bus_source);
    g_signal_connect (G_OBJECT (bus), "message::error", (GCallback)error_cb, data);
    g_signal_connect (G_OBJECT (bus), "message::state-changed", (GCallback)state_changed_cb, data);
    gst_object_unref (bus);

    LOGD("Entering Main Loop...");
    data->main_loop = g_main_loop_new (data->context, FALSE);
    check_initialization_complete (data);
    g_main_loop_run (data->main_loop);
    LOGD ("Exited main loop");
    g_main_loop_unref (data->main_loop);
    data->main_loop = NULL;

    g_main_context_pop_thread_default(data->context);
    g_main_context_unref(data->context);
    gst_element_set_state(data->pipeline, GST_STATE_NULL);
    gst_object_unref(data->video_sink);
    gst_object_unref(data->pipeline);

    return NULL;
}


void gst_native_surface_init(JNIEnv *env, jobject thiz, jobject surface)
{
    CustomData *data = GET_CUSTOM_DATA(env,thiz, custom_data_field_id);
    if(!data) return;
    ANativeWindow *new_native_window = ANativeWindow_fromSurface(env,surface);
    LOGD ("Received surface %p (native window %p)", surface, new_native_window);

    if(data->native_window) {
        ANativeWindow_release(data->native_window);
        if(data->native_window == new_native_window)
        {
            LOGD ("New native window is the same as the previous one %p", data->native_window);
            if(data->video_sink) {
                gst_video_overlay_expose(GST_VIDEO_OVERLAY(data->video_sink));
                gst_video_overlay_expose(GST_VIDEO_OVERLAY(data->video_sink));
            }
            return;
        }
        else
        {
            LOGD("Released prevous native window %p", data->native_window);
            data->initialized = FALSE;
        }
    }
    data->native_window = new_native_window;

    check_initialization_complete(data);
}


void gst_native_surface_finalize(JNIEnv *env, jobject thiz)
{
    CustomData *data = GET_CUSTOM_DATA (env, thiz, custom_data_field_id);
    if (!data) return;
    LOGD ("Releasing Native Window %p", data->native_window);

    if (data->video_sink) {
        gst_video_overlay_set_window_handle (GST_VIDEO_OVERLAY (data->video_sink), (guintptr)NULL);
        gst_element_set_state (data->pipeline, GST_STATE_READY);
    }

    ANativeWindow_release (data->native_window);
    data->native_window = NULL;
    data->initialized = FALSE;
}

void gst_native_init(JNIEnv *env, jobject thiz, jstring pipeline)
{
    const char *str = (*env)->GetStringUTFChars(env, pipeline, 0);
    strcpy(this_session_pipeline, str);
    (*env)->ReleaseStringChars(env, pipeline, (const jchar *) str);
    LOGD("current pipeline : %s", this_session_pipeline);

    CustomData *data = g_new0(CustomData, 1);
    SET_CUSTOM_DATA(env, thiz, custom_data_field_id, data);
    GST_DEBUG_CATEGORY_INIT (debug_category, "tutorial-2", 0, "Android tutorial 2");
    gst_debug_set_threshold_for_name("tutorial-2", GST_LEVEL_DEBUG);
    LOGD("Created CustomData at %p", data);
    data->app = (*env)->NewGlobalRef(env,thiz);
    LOGD("Created GlobalRef of this application at %p", data->app);
    pthread_create (&gst_app_thread, NULL, &app_function, data);
}


void gst_native_finalize(JNIEnv *env, jobject thiz)
{
    CustomData *data = GET_CUSTOM_DATA (env, thiz, custom_data_field_id);
    if (!data) return;
    LOGD ("Quitting main loop...");
    g_main_loop_quit (data->main_loop);
    LOGD ("Waiting for thread to finish...");
    pthread_join (gst_app_thread, NULL);
    LOGD ("Deleting GlobalRef for app object at %p", data->app);
    (*env)->DeleteGlobalRef (env, data->app);
    LOGD ("Freeing CustomData at %p", data);
    g_free (data);
    SET_CUSTOM_DATA (env, thiz, custom_data_field_id, NULL);
    LOGD ("Done finalizing");
}

void gst_native_play(JNIEnv *env, jobject thiz)
{
    CustomData *data = GET_CUSTOM_DATA(env,thiz, custom_data_field_id);
    if(!data) return;
    LOGD("Setting state to PLAYING");
    gst_element_set_state(data->pipeline, GST_STATE_PLAYING);
}

void gst_native_paused(JNIEnv *env, jobject thiz)
{
    CustomData *data = GET_CUSTOM_DATA(env,thiz, custom_data_field_id);
    if(!data) return;
    LOGD("Setting state to PAUSED");
    gst_element_set_state(data->pipeline, GST_STATE_PAUSED);
}

