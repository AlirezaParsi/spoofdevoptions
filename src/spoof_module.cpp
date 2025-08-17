// src/monitor_module.cpp
#include <jni.h>
#include <string>
#include <android/log.h>
#include "zygisk.hpp"

#define LOG_TAG "SpoofModule"
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)
#define ARRAYSIZE(a) (sizeof(a)/sizeof((a)[0]))

// ------------------ SystemProperties (native_*) ------------------
static jstring  (*orig_SP_native_get)(JNIEnv*, jclass, jstring);
static jint     (*orig_SP_native_get_int)(JNIEnv*, jclass, jstring, jint);
static jlong    (*orig_SP_native_get_long)(JNIEnv*, jclass, jstring, jlong);
static jboolean (*orig_SP_native_get_boolean)(JNIEnv*, jclass, jstring, jboolean);

static jstring hook_SP_native_get(JNIEnv* env, jclass clazz, jstring key) {
    const char* k = key ? env->GetStringUTFChars(key, nullptr) : "";
    LOGD("SystemProperties.native_get key=%s", k);
    if (key) env->ReleaseStringUTFChars(key, k);
    return orig_SP_native_get ? orig_SP_native_get(env, clazz, key) : nullptr;
}
static jint hook_SP_native_get_int(JNIEnv* env, jclass clazz, jstring key, jint def) {
    const char* k = key ? env->GetStringUTFChars(key, nullptr) : "";
    LOGD("SystemProperties.native_get_int key=%s def=%d", k, (int)def);
    if (key) env->ReleaseStringUTFChars(key, k);
    return orig_SP_native_get_int ? orig_SP_native_get_int(env, clazz, key, def) : def;
}
static jlong hook_SP_native_get_long(JNIEnv* env, jclass clazz, jstring key, jlong def) {
    const char* k = key ? env->GetStringUTFChars(key, nullptr) : "";
    LOGD("SystemProperties.native_get_long key=%s def=%lld", k, (long long)def);
    if (key) env->ReleaseStringUTFChars(key, k);
    return orig_SP_native_get_long ? orig_SP_native_get_long(env, clazz, key, def) : def;
}
static jboolean hook_SP_native_get_boolean(JNIEnv* env, jclass clazz, jstring key, jboolean def) {
    const char* k = key ? env->GetStringUTFChars(key, nullptr) : "";
    LOGD("SystemProperties.native_get_boolean key=%s def=%d", k, (int)def);
    if (key) env->ReleaseStringUTFChars(key, k);
    return orig_SP_native_get_boolean ? orig_SP_native_get_boolean(env, clazz, key, def) : def;
}

// ------------------ Settings.getStringForUser ------------------
static jstring (*orig_getStringForUser)(JNIEnv*, jclass, jobject, jstring, jint);

static jstring hook_getStringForUser(JNIEnv* env, jclass clazz, jobject cr, jstring name, jint user) {
    const char* n = name ? env->GetStringUTFChars(name, nullptr) : "";
    LOGD("Settings.getStringForUser key=%s user=%d", n, (int)user);
    if (name) env->ReleaseStringUTFChars(name, n);
    return orig_getStringForUser ? orig_getStringForUser(env, clazz, cr, name, user) : nullptr;
}

static void hook_settings_class(zygisk::Api* api, JNIEnv* env, const char* cls) {
    JNINativeMethod m[] = {
        {"getStringForUser", "(Landroid/content/ContentResolver;Ljava/lang/String;I)Ljava/lang/String;", (void*) hook_getStringForUser},
    };
    api->hookJniNativeMethods(env, cls, m, ARRAYSIZE(m));
    LOGD("Hooked %s.getStringForUser -> orig=%p", cls, (void*)m[0].fnPtr);
    if (!orig_getStringForUser) {
        orig_getStringForUser = (decltype(orig_getStringForUser)) m[0].fnPtr;
    }
}

class MonitorModule : public zygisk::ModuleBase {
public:
    void onLoad(zygisk::Api* api, JNIEnv* env) override {
        this->api = api; this->env = env;
    }

    void preAppSpecialize(zygisk::AppSpecializeArgs* args) override {
        const char* pkg = args->nice_name ? env->GetStringUTFChars(args->nice_name, nullptr) : "";
        LOGD("App detected: %s", pkg ? pkg : "(null)");

        // --- SystemProperties native_* ---
        {
            JNINativeMethod m[] = {
                {"native_get", "(Ljava/lang/String;)Ljava/lang/String;", (void*) hook_SP_native_get},
                {"native_get_int", "(Ljava/lang/String;I)I", (void*) hook_SP_native_get_int},
                {"native_get_long", "(Ljava/lang/String;J)J", (void*) hook_SP_native_get_long},
                {"native_get_boolean", "(Ljava/lang/String;Z)Z", (void*) hook_SP_native_get_boolean},
            };
            api->hookJniNativeMethods(env, "android/os/SystemProperties", m, ARRAYSIZE(m));
            orig_SP_native_get          = (decltype(orig_SP_native_get)) m[0].fnPtr;
            orig_SP_native_get_int      = (decltype(orig_SP_native_get_int)) m[1].fnPtr;
            orig_SP_native_get_long     = (decltype(orig_SP_native_get_long)) m[2].fnPtr;
            orig_SP_native_get_boolean  = (decltype(orig_SP_native_get_boolean)) m[3].fnPtr;
            LOGD("SP.native_get ptrs: get=%p get_int=%p get_long=%p get_bool=%p",
                 (void*)orig_SP_native_get, (void*)orig_SP_native_get_int,
                 (void*)orig_SP_native_get_long, (void*)orig_SP_native_get_boolean);
        }

        // --- Settings.*.getStringForUser on multiple classes ---
        hook_settings_class(api, env, "android/provider/Settings$Secure");
        hook_settings_class(api, env, "android/provider/Settings$System");
        hook_settings_class(api, env, "android/provider/Settings$Global");
        hook_settings_class(api, env, "android/provider/Settings$NameValueCache");

        if (pkg) env->ReleaseStringUTFChars(args->nice_name, pkg);
    }

private:
    zygisk::Api* api{};
    JNIEnv* env{};
};

REGISTER_ZYGISK_MODULE(MonitorModule)
