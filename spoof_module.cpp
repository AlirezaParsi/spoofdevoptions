#include <zygisk.hpp>
#include <jni.h>
#include <android/log.h>
#include <string>
#include <fcntl.h>
#include <unistd.h>
#include <fstream>
#include <sys/types.h>
#include <sys/stat.h>
#include <nlohmann/json.hpp>

#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, "spoofdevoptions", __VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN, "spoofdevoptions", __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, "spoofdevoptions", __VA_ARGS__)

using json = nlohmann::json;

static const char* TARGET_PACKAGE = "com.tosan.dara.sepah";
static const char* MODULE_ID = "spoofdevoptions";

class SpoofModule : public zygisk::ModuleBase {
public:
    void onLoad(zygisk::Api* api, JNIEnv* env) override {
        this->api = api;
        this->env = env;
        LOGD("Module loaded successfully");
    }

    void preAppSpecialize(zygisk::AppSpecializeArgs* args) override {
        if (!args || !args->nice_name) {
            LOGW("No nice_name in args");
            api->setOption(zygisk::Option::DLCLOSE_MODULE_LIBRARY);
            return;
        }

        jstring package_name = args->nice_name;
        const char* package_cstr = env->GetStringUTFChars(package_name, nullptr);
        if (!package_cstr) {
            LOGE("Failed to get package name");
            api->setOption(zygisk::Option::DLCLOSE_MODULE_LIBRARY);
            return;
        }

        std::string package(package_cstr);
        env->ReleaseStringUTFChars(package_name, package_cstr);

        if (package != TARGET_PACKAGE) {
            LOGD("Package %s is not target, closing module", package.c_str());
            api->setOption(zygisk::Option::DLCLOSE_MODULE_LIBRARY);
            return;
        }

        LOGD("Processing package: %s", TARGET_PACKAGE);

        // Load configuration
        json config = loadConfig();
        if (config.is_discarded()) {
            LOGE("Failed to load config, closing module");
            api->setOption(zygisk::Option::DLCLOSE_MODULE_LIBRARY);
            return;
        }

        // Hook Settings.Global, Settings.Secure, and SystemProperties
        hookSettingsMethods(config);
        hookSystemProperties(config);
    }

private:
    zygisk::Api* api;
    JNIEnv* env;

    struct Setting {
        const char* className;
        const char* settingKey;
        const char* preferenceKey;
    };

    struct Property {
        const char* propertyKey;
        const char* preferenceKey;
        const char* overrideValue;
        bool customOverride;
    };

    static const Setting settings[];
    static const Property props[];

    json loadConfig() {
        int module_fd = api->getModuleDir();
        if (module_fd < 0) {
            LOGE("Failed to get module directory: %d", module_fd);
            return json();
        }

        std::string config_path = "/proc/self/fd/" + std::to_string(module_fd) + "/config.json";
        LOGD("Attempting to load config from: %s", config_path.c_str());

        if (access(config_path.c_str(), R_OK) != 0) {
            LOGE("Cannot access config.json at %s: %s", config_path.c_str(), strerror(errno));
            return json();
        }

        std::ifstream config_file(config_path);
        if (!config_file.is_open()) {
            LOGE("Failed to open config.json at %s", config_path.c_str());
            return json();
        }

        json config;
        try {
            config = json::parse(config_file, nullptr, false);
            LOGD("Config loaded successfully");
        } catch (const json::exception& e) {
            LOGE("JSON parsing error: %s", e.what());
            config = json();
        }
        config_file.close();
        return config;
    }

    static jstring getStringForUserHook(JNIEnv* env, jobject, jstring key, jint) {
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
    }

    void hookSettingsMethods(const json& config) {
        for (const auto& setting : settings) {
            if (!config.value(setting.preferenceKey, true)) {
                LOGD("Skipping hook for %s (disabled in config)", setting.preferenceKey);
                continue;
            }

            JNINativeMethod methods[] = {
                {"getStringForUser", "(Ljava/lang/String;I)Ljava/lang/String;", (void*)getStringForUserHook}
            };
            api->hookJniNativeMethods(env, setting.className, methods, 1);
            LOGD("Hooked %s.getStringForUser", setting.className);
        }
    }

    static jstring getPropHook(JNIEnv* env, jclass, jstring key) {
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
    }

    static jboolean getBooleanPropHook(JNIEnv* env, jclass, jstring key, jboolean def) {
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
    }

    static jint getIntPropHook(JNIEnv* env, jclass, jstring key, jint def) {
        const char* key_cstr = env->GetStringUTFChars(key, nullptr);
        if (!key_cstr) return def;

        std::string key_str(key_cstr);
        env->ReleaseStringUTFChars(key, key_cstr);

        for (const auto& prop : props) {
            if (key_str == prop.propertyKey) {
                LOGD("Hooked SystemProperties.getInt for %s", prop.propertyKey);
                try {
                    return prop.customOverride && prop.propertyKey == std::string("sys.usb.ffs.ready") ? 0 : std::stoi(prop.overrideValue);
                } catch (...) {
                    return 0;
                }
            }
        }
        return def;
    }

    static jlong getLongPropHook(JNIEnv* env, jclass, jstring key, jlong def) {
        const char* key_cstr = env->GetStringUTFChars(key, nullptr);
        if (!key_cstr) return def;

        std::string key_str(key_cstr);
        env->ReleaseStringUTFChars(key, key_cstr);

        for (const auto& prop : props) {
            if (key_str == prop.propertyKey) {
                LOGD("Hooked SystemProperties.getLong for %s", prop.propertyKey);
                try {
                    return prop.customOverride && prop.propertyKey == std::string("sys.usb.ffs.ready") ? 0L : std::stol(prop.overrideValue);
                } catch (...) {
                    return 0L;
                }
            }
        }
        return def;
    }

    void hookSystemProperties(const json& config) {
        for (const auto& prop : props) {
            if (!config.value(prop.preferenceKey, true)) {
                LOGD("Skipping hook for %s (disabled in config)", prop.preferenceKey);
                continue;
            }

            JNINativeMethod methods[] = {
                {"get", "(Ljava/lang/String;)Ljava/lang/String;", (void*)getPropHook},
                {"get", "(Ljava/lang/String;Ljava/lang/String;)Ljava/lang/String;", (void*)getPropHook},
                {"getBoolean", "(Ljava/lang/String;Z)Z", (void*)getBooleanPropHook},
                {"getInt", "(Ljava/lang/String;I)I", (void*)getIntPropHook},
                {"getLong", "(Ljava/lang/String;J)J", (void*)getLongPropHook},
            };
            api->hookJniNativeMethods(env, "android.os.SystemProperties", methods, 5);
            LOGD("Hooked SystemProperties for %s", prop.propertyKey);
        }
    }
};

// Define static members outside the class
const SpoofModule::Setting SpoofModule::settings[] = {
    {"android.provider.Settings$Global", "development_settings_enabled", "development_settings_enabled"},
    {"android.provider.Settings$Secure", "development_settings_enabled", "development_settings_enabled_legacy"},
    {"android.provider.Settings$Global", "adb_enabled", "adb_enabled"},
    {"android.provider.Settings$Secure", "adb_enabled", "adb_enabled_legacy"},
    {"android.provider.Settings$Global", "adb_wifi_enabled", "adb_wifi_enabled"},
};

const SpoofModule::Property SpoofModule::props[] = {
    {"sys.usb.state", "adb_system_props_usb_state", "mtp", false},
    {"sys.usb.config", "adb_system_props_usb_config", "mtp", false},
    {"persist.sys.usb.reboot.func", "adb_system_props_reboot_func", "mtp", false},
    {"init.svc.adbd", "adb_system_props_svc_adbd", "stopped", false},
    {"sys.usb.ffs.ready", "adb_system_props_ffs_ready", "0", true},
};

REGISTER_ZYGISK_MODULE(SpoofModule)
