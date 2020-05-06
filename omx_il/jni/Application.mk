# APP_ABI := armeabi-v7a arm64-v8a x86 x86_64
APP_ABI := arm64-v8a x86_64

# xiaofeng: set default platform android 7.1+
APP_PLATFORM = android-25

# Disable PIE for SDK <16 support. Enable manually for >=5.0
# where necessary.
APP_PIE := false
