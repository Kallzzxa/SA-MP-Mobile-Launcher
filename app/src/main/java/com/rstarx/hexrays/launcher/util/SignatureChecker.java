package com.rstarx.hexrays.launcher.util;

import android.content.Context;

public class SignatureChecker {
    public static boolean isSignatureValid(Context context, String packageName) {
        // Luôn trả về true để bỏ qua mọi bước kiểm tra chữ ký gốc
        return true; 
    }
}

