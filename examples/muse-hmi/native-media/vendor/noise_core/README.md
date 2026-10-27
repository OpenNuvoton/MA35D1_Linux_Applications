# Muse Noise core

The unmodified portable protocol headers and six source files used by the native
MA35 gateway come from Meta's [muse-gadget-sdk](https://github.com/facebookincubator/muse-gadget-sdk/tree/b139b45064b4dcecf7bfe97e75bc7f99c10c28b6/esp32/components/noise_core)
at commit `b139b45064b4dcecf7bfe97e75bc7f99c10c28b6`. They retain their Apache-2.0
license and original copyright notices. Platform-specific ESP32 crypto backends
are not compiled; `openssl_backend.hpp` supplies the Linux crypto backend.
