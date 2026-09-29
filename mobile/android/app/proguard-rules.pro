# WebRTC calls back into Java from native code (JNI): keep its classes and members.
-keep class org.webrtc.** { *; }
-dontwarn org.webrtc.**

# secp256k1-kmp loads its JNI implementation by name.
-keep class fr.acinq.secp256k1.** { *; }

# OkHttp's optional platform integrations are looked up by reflection.
-dontwarn okhttp3.internal.platform.**
-dontwarn org.conscrypt.**
-dontwarn org.bouncycastle.**
-dontwarn org.openjsse.**
