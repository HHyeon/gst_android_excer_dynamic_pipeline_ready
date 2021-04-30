
#include <jni.h>

#include "global.h"
#include "gst_worker_native.h"

void native_side_test(JNIEnv *env, jobject thiz)
{
    GstRegistry *registry = gst_registry_get();
    GList *plugins = gst_registry_get_plugin_list(registry);

    GList *features, *f;
    GList *p;
    for(p=plugins;p;p=p->next)
    {
        GstPlugin *plugin = p->data;
        __android_log_print(ANDROID_LOG_DEBUG , "elements", "%s", gst_plugin_get_name(plugin));

        features = gst_registry_get_feature_list_by_plugin(registry, gst_plugin_get_name(plugin));
        for(f=features;f;f=f->next)
        {
            GstPluginFeature *feature = f->data;
            __android_log_print(ANDROID_LOG_DEBUG , "elements", "\t%s", gst_plugin_feature_get_name(feature));
        }
    }
    gst_plugin_list_free(plugins);
}

jstring native_gst_get_version(JNIEnv *env, jobject thiz)
{
    return (*env)->NewStringUTF(env, gst_version_retreieve());
}

pthread_t gst_app_thread;
pthread_key_t current_jni_env;
JavaVM *java_vm = NULL;


extern void gst_native_init(JNIEnv *env, jobject thiz, jstring pipeline);
extern void gst_native_finalize(JNIEnv *env, jobject thiz);
extern jboolean gst_native_class_init(JNIEnv *env, jclass clazz);
extern void detach_current_thread (void *env);
extern void gst_native_play(JNIEnv *env, jobject thiz);
extern void gst_native_paused(JNIEnv *env, jobject thiz);
extern void gst_native_surface_init(JNIEnv *env, jobject thiz, jobject surface);
extern void gst_native_surface_finalize(JNIEnv *env, jobject thiz);


static JNINativeMethod native_methods[] = {
        {"element_list", "()V", (void*)native_side_test},
        {"gst_version_from_jni", "()Ljava/lang/String;", (jstring*)native_gst_get_version},
        {"nativeInit", "(Ljava/lang/String;)V", (void*)gst_native_init},
        {"nativeFinalize", "()V", (void*)gst_native_finalize},
        { "nativeClassInit", "()Z", (void *) gst_native_class_init},
        { "nativePaused", "()V", (void *) gst_native_paused},
        { "nativePlay", "()V", (void *) gst_native_play},
        { "nativeSurfaceInit", "(Ljava/lang/Object;)V", (void *) gst_native_surface_init},
        { "nativeSurfaceFinalize", "()V", (void *) gst_native_surface_finalize},
};

jint JNI_OnLoad(JavaVM *vm, void *reserved)
{
    JNIEnv *env = NULL;

    java_vm = vm;

    if ((*vm)->GetEnv(vm, (void**) &env, JNI_VERSION_1_6) != JNI_OK) {
        LOGE("Could not retrieve JNIEnv");
        return 0;
    }

    jclass klass = (*env)->FindClass(env,"com/example/gst_android_excer/MainActivity");
    (*env)->RegisterNatives(env,klass,native_methods,sizeof(native_methods)/sizeof(native_methods[0]));

    pthread_key_create (&current_jni_env, detach_current_thread);

    return JNI_VERSION_1_6;
}