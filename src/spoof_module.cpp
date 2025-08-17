#include <jni.h>
#include <string>
#include <android/log.h>
#include "zygisk.hpp"

#define LOG_TAG "SpoofModule"
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)

// ------------------------- SystemProperties -------------------------
static jstring (*orig_SP_get1)(JNIEnv*, jclass, jstring);
static jstring (*orig_SP_get2)(JNIEnv*, jclass, jstring, jstring);
static jboolean (*orig_SP_getBoolean)(JNIEnv*, jclass, jstring, jboolean);
static jint (*orig_SP_getInt)(JNIEnv*, jclass, jstring, jint);
static jlong (*orig_SP_getLong)(JNIEnv*, jclass, jstring, jlong);

static jstring hook_SP_get1(JNIEnv* env, jclass clazz, jstring key) {
    const char* k = env->GetStringUTFChars(key, nullptr);
    LOGD("SystemProperties.get(String) key=%s", k);
    env->ReleaseStringUTFChars(key, k);

    return orig_SP_get1 ? orig_SP_get1(env, clazz, key) : nullptr;
}

static jstring hook_SP_get2(JNIEnv* env, jclass clazz, jstring key, jstring def) {
    const char* k = env->GetStringUTFChars(key, nullptr);
    LOGD("SystemProperties.get(String,String) key=%s", k);
    env->ReleaseStringUTFChars(key, k);

    return orig_SP_get2 ? orig_SP_get2(env, clazz, key, def) : def;
}

static jboolean hook_SP_getBoolean(JNIEnv* env, jclass clazz, jstring key, jboolean def) {
    const char* k = env->GetStringUTFChars(key, nullptr);
    LOGD("SystemProperties.getBoolean key=%s", k);
    env->ReleaseStringUTFChars(key, k);

    return orig_SP_getBoolean ? orig_SP_getBoolean(env, clazz, key, def) : def;
}

static jint hook_SP_getInt(JNIEnv* env, jclass clazz, jstring key, jint def) {
    const char* k = env->GetStringUTFChars(key, nullptr);
    LOGD("SystemProperties.getInt key=%s", k);
    env->ReleaseStringUTFChars(key, k);

    return orig_SP_getInt ? orig_SP_getInt(env, clazz, key, def) : def;
}

static jlong hook_SP_getLong(JNIEnv* env, jclass clazz, jstring key, jlong def) {
    const char* k = env->GetStringUTFChars(key, nullptr);
    LOGD("SystemProperties.getLong key=%s", k);
    env->ReleaseStringUTFChars(key, k);

    return orig_SP_getLong ? orig_SP_getLong(env, clazz, key, def) : def;
}

// ------------------------- Settings -------------------------
static jint (*orig_Settings_getInt1)(JNIEnv*, jclass, jobject, jstring);
static jint (*orig_Settings_getInt2)(JNIEnv*, jclass, jobject, jstring, jint);
static jstring (*orig_Settings_getStringForUser)(JNIEnv*, jclass, jobject, jstring, jint);

static jint hook_Settings_getInt1(JNIEnv* env, jclass clazz, jobject cr, jstring name) {
    const char* k = env->GetStringUTFChars(name, nullptr);
    LOGD("Settings.getInt(cr,key) key=%s", k);
    env->ReleaseStringUTFChars(name, k);

    return orig_Settings_getInt1 ? orig_Settings_getInt1(env, clazz, cr, name) : 0;
}

static jint hook_Settings_getInt2(JNIEnv* env, jclass clazz, jobject cr, jstring name, jint def) {
    const char* k = env->GetStringUTFChars(name, nullptr);
    LOGD("Settings.getInt(cr,key,def) key=%s", k);
    env->ReleaseStringUTFChars(name, k);

    return orig_Settings_getInt2 ? orig_Settings_getInt2(env, clazz, cr, name, def) : def;
}

static jstring hook_Settings_getStringForUser(JNIEnv* env, jclass clazz, jobject cr, jstring name, jint user) {
    const char* k = env->GetStringUTFChars(name, nullptr);
    LOGD("Settings.getStringForUser key=%s user=%d", k, user);
    env->ReleaseStringUTFChars(name, k);

    return orig_Settings_getStringForUser ? orig_Settings_getStringForUser(env, clazz, cr, name, user) : nullptr;
}

// ------------------------- Module -------------------------
class SpoofModule : public zygisk::ModuleBase {
public:
    void onLoad(zygisk::Api* api, JNIEnv* env) override {
        this->api = api;
        this->env = env;
    }

    void preAppSpecialize(zygisk::AppSpecializeArgs* args) override {
        const char* pkg = args->nice_name ? env->GetStringUTFChars(args->nice_name, nullptr) : "";
        LOGD("App detected: %s", pkg);

        // فقط اگه خواستی روی یک اپ خاص فعال بشه، شرط بگذار:
        // if (!pkg || std::string(pkg) != "com.example.app") return;

        // ---- Hook SystemProperties ----
        {
            JNINativeMethod m[] = {
                {"get", "(Ljava/lang/String;)Ljava/lang/String;", (void*) hook_SP_get1},
                {"get", "(Ljava/lang/String;Ljava/lang/String;)Ljava/lang/String;", (void*) hook_SP_get2},
                {"getBoolean", "(Ljava/lang/String;Z)Z", (void*) hook_SP_getBoolean},
                {"getInt", "(Ljava/lang/String;I)I", (void*) hook_SP_getInt},
                {"getLong", "(Ljava/lang/String;J)J", (void*) hook_SP_getLong},
            };
            api->hookJniNativeMethods(env, "android/os/SystemProperties", m, 5);
            orig_SP_get1 = (decltype(orig_SP_get1)) m[0].fnPtr;
            orig_SP_get2 = (decltype(orig_SP_get2)) m[1].fnPtr;
            orig_SP_getBoolean = (decltype(orig_SP_getBoolean)) m[2].fnPtr;
            orig_SP_getInt = (decltype(orig_SP_getInt)) m[3].fnPtr;
            orig_SP_getLong = (decltype(orig_SP_getLong)) m[4].fnPtr;
        }

        // ---- Hook Settings.Global ----
        {
            JNINativeMethod m[] = {
                {"getInt", "(Landroid/content/ContentResolver;Ljava/lang/String;)I", (void*) hook_Settings_getInt1},
                {"getInt", "(Landroid/content/ContentResolver;Ljava/lang/String;I)I", (void*) hook_Settings_getInt2},
                {"getStringForUser", "(Landroid/content/ContentResolver;Ljava/lang/String;I)Ljava/lang/String;", (void*) hook_Settings_getStringForUser},
            };
            api->hookJniNativeMethods(env, "android/provider/Settings$Global", m, 3);
            orig_Settings_getInt1 = (decltype(orig_Settings_getInt1)) m[0].fnPtr;
            orig_Settings_getInt2 = (decltype(orig_Settings_getInt2)) m[1].fnPtr;
            orig_Settings_getStringForUser = (decltype(orig_Settings_getStringForUser)) m[2].fnPtr;
        }

        // ---- Hook Settings.Secure ----
        {
            JNINativeMethod m[] = {
                {"getInt", "(Landroid/content/ContentResolver;Ljava/lang/String;)I", (void*) hook_Settings_getInt1},
                {"getInt", "(Landroid/content/ContentResolver;Ljava/lang/String;I)I", (void*) hook_Settings_getInt2},
                {"getStringForUser", "(Landroid/content/ContentResolver;Ljava/lang/String;I)Ljava/lang/String;", (void*) hook_Settings_getStringForUser},
            };
            api->hookJniNativeMethods(env, "android/provider/Settings$Secure", m, 3);
        }

        if (pkg) env->ReleaseStringUTFChars(args->nice_name, pkg);
    }

private:
    zygisk::Api* api{};
    JNIEnv* env{};
};

REGISTER_ZYGISK_MODULE(SpoofModule)
