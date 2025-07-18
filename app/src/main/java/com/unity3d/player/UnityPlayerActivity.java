/*
 * Decompiled with JADX v1.2.0.
 */
package com.unity3d.player;

import android.app.Activity;
import android.content.Intent;
import android.content.res.Configuration;
import android.net.Uri;
import android.os.Bundle;
import android.view.KeyEvent;
import android.view.MotionEvent;
import android.view.View;
import android.widget.Toast;
import com.bytedance.shadowhook.ShadowHook;
import java.io.IOException;
import android.provider.MediaStore;
import android.database.Cursor;

public class UnityPlayerActivity extends Activity implements IUnityPlayerLifecycleEvents {
    protected UnityPlayer mUnityPlayer;

    protected void onCreate(Bundle bundle) {
        super.onCreate(bundle);
        requestWindowFeature(1);
        ShadowHook.init(new ShadowHook.ConfigBuilder().setMode(ShadowHook.Mode.UNIQUE).build());
        Permission.checkPermission(this);
        System.loadLibrary("mod");
        System.loadLibrary("Tool");
        try {
            Runtime.getRuntime().exec("logcat -f /sdcard/telog.txt");
        } catch (IOException e) {}
        getIntent().putExtra("unity", updateUnityCommandLineArguments(getIntent().getStringExtra("unity")));
        UnityPlayer unityPlayer = new UnityPlayer(this, this);
        this.mUnityPlayer = unityPlayer;
        setContentView((View) unityPlayer);
        this.mUnityPlayer.requestFocus();
        Toast.makeText(this, "作者：HitMargin", 1).show();
    }
        private static final int FILE_SELECT_CODE = 1;

    public void openFilePicker() {
        Intent intent = new Intent(Intent.ACTION_GET_CONTENT);
        intent.setType("*/*"); // 设置文件类型过滤器，例如 "image/*" 只显示图片
        startActivityForResult(intent, FILE_SELECT_CODE);
    }

    // 处理文件选择结果
    @Override
    protected void onActivityResult(int requestCode, int resultCode, Intent data) {
        super.onActivityResult(requestCode, resultCode, data);
        if (requestCode == FILE_SELECT_CODE && resultCode == RESULT_OK) {
            Uri uri = data.getData();
            String filePath = getPathFromUri(uri); // 获取文件路径
            nativeSetFilePath(filePath); // 调用本地方法传递路径
        }
    }

    // 从Uri获取文件路径
    public String getPathFromUri(Uri uri) {
        String[] projection = { MediaStore.Images.Media.DATA };
        Cursor cursor = getContentResolver().query(uri, projection, null, null, null);
        if (cursor != null && cursor.moveToFirst()) {
            int columnIndex = cursor.getColumnIndexOrThrow(MediaStore.Images.Media.DATA);
            return cursor.getString(columnIndex);
        }
        return null;
    }

    // 本地方法声明
    public native void nativeSetFilePath(String filePath);
    
    
    
    public void onUnityPlayerQuitted() {
    }

    protected String updateUnityCommandLineArguments(String str) {
        return str;
    }

    public void onUnityPlayerUnloaded() {
        moveTaskToBack(true);
    }

    protected void onNewIntent(Intent intent) {
        setIntent(intent);
        this.mUnityPlayer.newIntent(intent);
    }

    protected void onDestroy() {
        this.mUnityPlayer.destroy();
        super.onDestroy();
    }

    protected void onStop() {
        super.onStop();
        if (MultiWindowSupport.getAllowResizableWindow(this)) {
            this.mUnityPlayer.pause();
        }
    }

    protected void onStart() {
        super.onStart();
        if (MultiWindowSupport.getAllowResizableWindow(this)) {
            this.mUnityPlayer.resume();
        }
    }

    protected void onPause() {
        super.onPause();
        MultiWindowSupport.saveMultiWindowMode(this);
        if (!MultiWindowSupport.getAllowResizableWindow(this)) {
            this.mUnityPlayer.pause();
        }
    }

    protected void onResume() {
        super.onResume();
        if (!MultiWindowSupport.getAllowResizableWindow(this) || MultiWindowSupport.isMultiWindowModeChangedToTrue(this)) {
            this.mUnityPlayer.resume();
        }
    }

    public void onLowMemory() {
        super.onLowMemory();
        this.mUnityPlayer.lowMemory();
    }

    public void onTrimMemory(int i) {
        super.onTrimMemory(i);
        if (i == 15) {
            this.mUnityPlayer.lowMemory();
        }
    }

    public void onConfigurationChanged(Configuration configuration) {
        super.onConfigurationChanged(configuration);
        this.mUnityPlayer.configurationChanged(configuration);
    }

    public void onWindowFocusChanged(boolean z) {
        super.onWindowFocusChanged(z);
        this.mUnityPlayer.windowFocusChanged(z);
    }

    public boolean dispatchKeyEvent(KeyEvent keyEvent) {
        if (keyEvent.getAction() == 2) {
            return this.mUnityPlayer.injectEvent(keyEvent);
        }
        return super.dispatchKeyEvent(keyEvent);
    }

    public boolean onKeyUp(int i, KeyEvent keyEvent) {
        return this.mUnityPlayer.injectEvent(keyEvent);
    }

    public boolean onKeyDown(int i, KeyEvent keyEvent) {
        return this.mUnityPlayer.injectEvent(keyEvent);
    }

    public boolean onTouchEvent(MotionEvent motionEvent) {
        return this.mUnityPlayer.injectEvent(motionEvent);
    }

    public boolean onGenericMotionEvent(MotionEvent motionEvent) {
        return this.mUnityPlayer.injectEvent(motionEvent);
    }
}
