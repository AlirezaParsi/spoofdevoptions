#include <zygisk.hpp>
#include <jni.h>
#include <android/log.h>
#include <string>
#include <fcntl.h>
#include <unistd.h>
#include <nlohmann/json.hpp>
#include <sys/stat.h>
#include <vector>

#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, "spoofdevoptions", __VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN, "spoofdevoptions", __VA_ARGS__)

using json = nlohmann::json;

static const char* TARGET_PACKAGE = "com.tosan.dara.sepah";
static const char* MODULE_ID = "spoofdevoptions";

class SpoofModule : public zygisk::ModuleBase {
public:
    void onLoad(zygisk::Api* api, JNIEnv* env) override {
        this->api = api;
        this->env = env;
    }

    void preAppSpecialize(zygisk::AppSpecializeArgs* args) override {
        if (!args || !args->nice_name) {
            LOGW("No nice_name in args");
            return;
        }

        jstring package_name = args->nice_name;
        const char* package_cstr = env->GetStringUTFChars(package_name, nullptr);
        if (!package_cstr) {
            LOGW("Failed to get package name");
            return;
        }

        std::string package(package_cstr);
        env->ReleaseStringUTFChars(package_name, package_cstr);

        if (package != TARGET_PACKAGE) {
            api->setOption(zygisk::Option::DLCLOSE_MODULE_LIBRARY);
            return;
        }

        LOGD("Processing package: %s", TARGET_PACKAGE);

        // Load configuration
        int module_fd = api->getModuleDir();
        if (module_fd < 0) {
            LOGW("Failed to get module directory");
            return;
        }

        std::string config_path = "/proc/self/fd/" + std::to_string(module_fd) + "/config.json";
        std::ifstream config_file(config_path);
        json config;
        if (config_file.is_open()) {
            config = json::parse(config_file, nullptr, false);
            config_file.close();
            if (config.is_discarded()) {
                LOGW("Failed to parse config.json");
                return;
            }
        } else {
            LOGW("Failed to open config.json");
            return;
        }

        // Hook Settings.Global and Settings.Secure
        hookSettingsMethods(config);
        hookSystemProperties(config);
    }

private:
    zygisk::Api* api;
    JNIEnv* env;

    void hookSettingsMethods(const json& config) {
        struct {
            const char* className;
            const char* settingKey;
            const char* preferenceKey;
        } settings[] = {
            {"android.provider.Settings$Global", "development_settings_enabled", "development_settings_enabled"},
            {"android.provider.Settings$Secure", "development_settings_enabled", "development_settings_enabled_legacy"},
            {"android.provider.Settings$Global", "adb_enabled", "adb_enabled"},
            {"android.provider.Settings$Secure", "adb_enabled", "adb_enabled_legacy"},
            {"android.provider.Settings$Global", "adb_wifi_enabled", "adb_wifi_enabled"},
        };

        static auto getStringForUserHook = [](JNIEnv* env, jobject, jstring key, jint) -> jstring {
            const char* key_cstr = env->GetStringUTFChars(key, nullptr);
            if (!key_cstr) return nullptr;

            std::string key_str(key_cstr);
            env->ReleaseStringUTFChars(key, key_cstr);

            for (const auto& setting : settings) {
                if (key_str == setting.settingKey) {
                    LOGD("Hooked getStringForUser for %s", setting.settingKey);
                    return env->NewStringUTF("0");
                }
            }
            return nullptr;
        };

        for (const auto& setting : settings) {
            if (!config.value(setting.preferenceKey, true)) continue;

            JNINativeMethod methods[] = {
                {"getStringForUser", "(Ljava/lang/String;I)Ljava/lang/String;", (void*)getStringForUserHook}
            };
            api->hookJniNativeMethods(env, setting.className, methods, 1);
            LOGD("Hooked %s.getStringForUser", setting.className);
        }
    }

    void hookSystemProperties(const json& config) {
        struct {
            const char* propertyKey;
            const char* preferenceKey;
            const char* overrideValue;
            bool customOverride;
        } props[] = {
            {"sys.usb.state", "adb_system_props_usb_state", "mtp", false},
            {"sys.usb.config", "adb_system_props_usb_config", "mtp", false},
            {"persist.sys.usb.reboot.func", "adb_system_props_reboot_func", "mtp", false},
            {"init.svc.adbd", "adb_system_props_svc_adbd", "stopped", false},
            {"sys.usb.ffs.ready", "adb_system_props_ffs_ready", "0", true},
        };

        static auto getHook = [](JNIEnv* env, jclass, jstring key) -> jstring {
            const char* key_cstr = env->GetStringUTFChars(key, nullptr);
            if (!key_cstr) return nullptr;

            std::string key_str(key_cstr);
            env->ReleaseStringUTFChars(key, key_cstr);

            for (const auto& prop : props) {
                if (key_str == prop.propertyKey) {
                    LOGD("Hooked SystemProperties.get for %s", prop.propertyKey);
                    return env->NewStringUTF(prop.overrideValue);
                }
            }
            return nullptr;
        };

        static auto getBooleanHook = [](JNIEnv*, jclass, jstring key, jboolean def) -> jboolean {
            const char* key_cstr = env->GetStringUTFChars(key, nullptr);
            if (!key_cstr) return def;

            std::string key_str(key_cstr);
            env->ReleaseStringUTFChars(key, key_cstr);

            for (const auto& prop : props) {
                if (key_str == prop.propertyKey) {
                    LOGD("Hooked SystemProperties.getBoolean for %s", prop.propertyKey);
                    return prop.customOverride && prop.propertyKey == std::string("sys.usb.ffs.ready") ? JNI_FALSE : (prop.overrideValue == std::string("true") ? JNI_TRUE : JNI_FALSE);
                }
            }
            return def;
        };

        static auto getIntHook = [](JNIEnv* env, jclass, jstring key, jint def) -> jint {
            const char* key_cstr = env->GetStringUTFChars(key, nullptr);
            if (!key_cstr) return def;

            std::string key_str(key_cstr);
            env->ReleaseStringUTFChars(key, key_cstr);

            for (const auto& prop : props) {
                if (key_str == prop.propertyKey) {
                    LOGD("Hooked SystemProperties.getInt for %s", prop.propertyKey);
                    return prop.customOverride && prop.propertyKey == std::string("sys.usb.ffs.ready") ? 0 : std::stoi(prop.overrideValue, nullptr, 10);
                }
            }
            return def;
        };

        static auto getLongHook = [](JNIEnv* env, jclass, jstring key, jlong def) -> jlong {
            const char* key_cstr = env->GetStringUTFChars(key, nullptr);
            if (!key_cstr) return def;

            std::string key_str(key_cstr);
            env->ReleaseStringUTFChars(key, key_cstr);

            for (const auto& prop : props) {
                if (key_str == prop.propertyKey) {
                    LOGD("Hooked SystemProperties.getLong for %s", prop.propertyKey);
                    return prop.customOverride && prop.propertyKey == std::string("sys.usb.ffs.ready") ? 0L : std::stol(prop.overrideValue, nullptr, 10);
                }
            }
            return def;
        };

        JNINativeMethod methods[] = {
            {"get", "(Ljava/lang/String;)Ljava/lang/String;", (void*)getHook},
            {"get", "(Ljava/lang/String;Ljava/lang/String;)Ljava/lang/String;", (void*)getHook},
            {"getBoolean", "(Ljava/lang/String;Z)Z", (void*)getBooleanHook},
            {"getInt", "(Ljava/lang/String;I)I", (void*)getIntHook},
            {"getLong", "(Ljava/lang/String;J)J", (void*)getLongHook},
        };

        for (const auto& prop : props) {
            if (!config.value(prop.preferenceKey, true)) continue;
            api->hookJniNativeMethods(env, "android.os.SystemProperties", methods, 5);
            LOGD("Hooked SystemProperties for %s", prop.propertyKey);
        }
    }
};

REGISTER_ZYGISK_MODULE(SpoofModule)
