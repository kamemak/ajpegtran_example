package github.kamemak.ajpegtran_example;

import android.Manifest;
import android.app.Dialog;
import android.content.ContentResolver;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.database.Cursor;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.os.ParcelFileDescriptor;
import android.provider.DocumentsContract;
import android.provider.OpenableColumns;
import android.view.View;
import android.widget.Button;
import android.widget.TextView;
import android.widget.Toast;

import androidx.activity.result.ActivityResult;
import androidx.activity.result.ActivityResultCallback;
import androidx.activity.result.ActivityResultLauncher;
import androidx.activity.result.contract.ActivityResultContracts;
import androidx.annotation.NonNull;
import androidx.annotation.RequiresApi;
import androidx.appcompat.app.AlertDialog;
import androidx.appcompat.app.AppCompatActivity;
import androidx.fragment.app.DialogFragment;
import androidx.fragment.app.FragmentManager;

import java.io.FileNotFoundException;
import java.io.IOException;
import java.io.OutputStream;

/**
 * Example for using jpegtran from Android.
 */
public class MainActivity extends AppCompatActivity {
    /* for jpeg_colorspace */
    static final int JCS_UNKNOWN=0;		/* error/unspecified */
    static final int JCS_GRAYSCALE=1;		/* monochrome */
    static final int JCS_RGB=2;		/* red/green/blue, standard RGB (sRGB) */
    static final int JCS_YCbCr=3;		/* Y/Cb/Cr (also known as YUV), standard YCC */
    static final int JCS_CMYK=4;		/* C/M/Y/K */
    static final int JCS_YCCK=5;		/* Y/Cb/Cr/K */
    static final int JCS_BG_RGB=6;		/* big gamut red/green/blue, bg-sRGB */
    static final int JCS_BG_YCC=7;		/* big gamut Y/Cb/Cr, bg-sYCC */

    static Uri loadUri = null;
    static String propertyStr = null;
    private ActivityResultLauncher<Intent> startForResultOpen,startForResultSave;

    private final int REQUEST_CODE_WRITE_PERMISSION = 0x01;

    // JNI for ajpegtran
    public native String ajpegtran(int rfd,int wfd,String optionstr);
    public native String ajpegtranhead(int fd,int []retarry);
    static {
        System.loadLibrary("ajpegtran");
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);

        // Register handler for tapping [OPEN] button
        Button button_open = (Button)findViewById(R.id.button_open);
        button_open.setOnClickListener(new View.OnClickListener() {
            public void onClick(View v){
                if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q &&
                        getApplicationContext().checkSelfPermission(Manifest.permission.ACCESS_MEDIA_LOCATION) != PackageManager.PERMISSION_GRANTED) {
                    // Android 10~ : Request permission to access GEOTAG.
                    requestPermissions(new String[]{Manifest.permission.ACCESS_MEDIA_LOCATION}, REQUEST_CODE_WRITE_PERMISSION);
                }
                else {
                    // Android 4.4~9 : No request is needed.
                    //  or
                    // The permission is granted
                    //  -> Open file selector
                    callFileSelection();
                }
                // If you access file without SAF, you must get permission WRITE_EXTERNAL_STORAGE.
            }
        });

        // Register handler for tapping [SAVE] button
        //  -> Open file selector
        Button button_save = (Button)findViewById(R.id.button_save);
        button_save.setOnClickListener(new View.OnClickListener() {
            public void onClick(View v){
                if( loadUri != null ) {
                    callFileSelectionCreate();
                }
            }
        });

        if( loadUri == null ) {
            // If file is not specified yet, disable [SAVE] button.
            button_save.setVisibility(View.INVISIBLE);
        }
        else if( propertyStr != null ){
            // If propertystr is exist, display it.
            TextView textView = (TextView)findViewById(R.id.text_view);
            textView.setText(propertyStr);
        }

        // Register handler for tapping [ABOUT] button
        //  -> Display app information dialog
        Button button_about = (Button)findViewById(R.id.button_about);
        button_about.setOnClickListener(new View.OnClickListener() {
            public void onClick(View v){
                FragmentManager manager = getSupportFragmentManager();
                AboutFragmentDialog abfdialog = new AboutFragmentDialog();
                abfdialog.show(manager, "dialog");
            }
        });

        // Register handler for open file selector
        //  -> Get JPEG property and display it
        startForResultOpen = registerForActivityResult(
                new ActivityResultContracts.StartActivityForResult(),
                new ActivityResultCallback<ActivityResult>() {
                    @Override
                    public void onActivityResult(ActivityResult result) {
                        if(result.getResultCode() == RESULT_OK ){
                            Intent data = result.getData();
                            if(data != null) {
                                Uri lloadUri = (Uri)data.getData();

                                // Call ajpegtranhead to get JPEG properties.
                                String jniresult;   // If success, "OK" is returned. If fail, error message is returned to here.
                                int[] retarray = new int[10];   // JPEG properties are returned to here.
                                try {
                                    ParcelFileDescriptor parcelFd = getContentResolver().openFileDescriptor(lloadUri, "r");
                                    // To simplify the sample code, call ajpegtranhead() from GUI thread directly.
                                    // When you implement your app, you should make thread and call ajpegtran() from the thread.
                                    jniresult = ajpegtranhead(parcelFd.detachFd(), retarray);
                                } catch (IOException | SecurityException e) {
                                    jniresult = getString(R.string.mess_error_readfile);
                                }

                                if (jniresult.startsWith("OK")) {
                                    loadUri = lloadUri;
                                    button_save.setVisibility(View.VISIBLE);

                                    //Get file name and file size
                                    Cursor returnCursor =
                                            getContentResolver().query(loadUri, null, null, null, null);
                                    String filename = null;
                                    long filesize = 0;
                                    if( returnCursor != null ) {
                                        int index = returnCursor.getColumnIndex(OpenableColumns.DISPLAY_NAME);
                                        returnCursor.moveToFirst();
                                        filename = returnCursor.getString(index);
                                        index = returnCursor.getColumnIndex(OpenableColumns.SIZE);
                                        filesize = returnCursor.getLong(index);
                                        returnCursor.close();
                                    }

                                    // Get properties from return array.
                                    int sizex = retarray[0];
                                    int sizey = retarray[1];
                                    int component_num = retarray[2];
                                    int mcux = retarray[3] * 8;
                                    int mcuy = retarray[4] * 8;
                                    int color_space = retarray[5];
                                    String []color_space_str = {"Unknown","Grayscale","RGB","YCbCr","CMYK","YCbCrK","RGB","YCbCr"};

                                    // Display properties
                                    TextView textView = (TextView)findViewById(R.id.text_view);
                                    propertyStr  = "File name : "+filename+"\n";
                                    propertyStr += "File size : "+filesize+"\n";
                                    propertyStr += "Width : "+sizex+"\n"+"Height : "+sizey+"\n";
                                    propertyStr += "MCU Width : "+mcux+"\n"+"MCU Height : "+mcuy+"\n";
                                    if( color_space <= JCS_BG_YCC ){
                                        propertyStr += "Color space : "+color_space_str[color_space]+"\n";
                                    }
                                    textView.setText(propertyStr);
                                }
                                else{
                                    // When fail to get property, display error message.
                                    Toast.makeText(getApplicationContext(), jniresult, Toast.LENGTH_LONG).show();
                                }
                            }
                        }
                    }
                });

        // Register handler for save file selector
        //  -> Execute Jpegtran
        startForResultSave = registerForActivityResult(
                new ActivityResultContracts.StartActivityForResult(),
                new ActivityResultCallback<ActivityResult>() {
                    @Override
                    public void onActivityResult(ActivityResult result) {
                        if(result.getResultCode() == RESULT_OK ){
                            Intent data = result.getData();
                            if(data != null) {
                                Uri saveUri = (Uri)data.getData();
                                ContentResolver resolver = getContentResolver();
                                String jniresult;
                                // Call ajpegtran with option string
                                try {
                                    ParcelFileDescriptor wparcelFd = resolver.openFileDescriptor(saveUri, "w");
                                    ParcelFileDescriptor rparcelFd = resolver.openFileDescriptor(loadUri, "r");
                                    // To simplify the sample code, call ajpegtran() from GUI thread directly.
                                    // When you implement your app, you should make thread and call ajpegtran() from the thread.
                                    // 
                                    // In the below, option "-grayscale -flip horizontal...' is specified,
                                    // so ajpegtran execute 'to grayscale' and "horizontal flip' operation.
                                    // All markers are copied and the huffman code is optimized.
                                    // Another operation can be executed with changing the option string.
                                    jniresult = ajpegtran(rparcelFd.detachFd(), wparcelFd.detachFd(), "-grayscale -flip horizontal -optimize -copy all");
                                } catch (IOException e) {
                                    jniresult = getString(R.string.mess_error_file);
                                }
                                if (jniresult.startsWith("OK")) {
                                    try {
                                        // Workaround : To update mediastore, execute empty write.
                                        OutputStream outstream = resolver.openOutputStream(saveUri, "wa");
                                        outstream.flush();
                                        outstream.close();
                                    }
                                    catch (IOException e){
                                        // Discard exception
                                        //  Mediastore may not be updated, but no recovery operation and continuable.
                                    }
                                    Toast.makeText(getApplicationContext(), getString(R.string.mess_success), Toast.LENGTH_LONG).show();
                                }
                                else{
                                    // When error is occurred, zero size file still remain.
                                    // It should be deleted.
                                    try {
                                        DocumentsContract.deleteDocument(resolver,saveUri);
                                    } catch (FileNotFoundException e) {
                                        // Discard exception
                                        //  The file is not exist, it is OK.
                                    }
                                    Toast.makeText(getApplicationContext(), jniresult, Toast.LENGTH_LONG).show();
                                }
                            }
                        }
                    }
                });
    }

    /**
     * Call file selector for reading JPEG file with readonly mode.
     */
    private void callFileSelection(){
        Intent intentGallery = new Intent(Intent.ACTION_GET_CONTENT);
        intentGallery.addCategory(Intent.CATEGORY_OPENABLE);
        intentGallery.setType("image/jpeg");
        startForResultOpen.launch(intentGallery);
    }

    /**
     * Call file selector for writing JPEG file with create mode.
     */
    private void callFileSelectionCreate() {
        Intent intentGallery = new Intent(Intent.ACTION_CREATE_DOCUMENT);
        intentGallery.addCategory(Intent.CATEGORY_OPENABLE);
        intentGallery.setType("image/jpeg");
        // Default file name
        intentGallery.putExtra(Intent.EXTRA_TITLE, "output.jpg");
        startForResultSave.launch(intentGallery);
    }

    /**
     * Handler after requesting permissions.
     */
    @RequiresApi(api = Build.VERSION_CODES.M)
    @Override
    public void onRequestPermissionsResult(int requestCode, @NonNull String[] permissions, int[] grantResults) {
        boolean locAccessDeny = false;
        // Check requested permission is granted or not.
        for( int count=0 ; count < grantResults.length ; count ++ ){
            if( permissions[count].equals(Manifest.permission.ACCESS_MEDIA_LOCATION) ){
                if( grantResults[count] != PackageManager.PERMISSION_GRANTED ){
                    locAccessDeny = true;
                }
            }
        }
        if( locAccessDeny ) {
            // When permission for accessing GEOTAGs is denied, warn it with toast.
            Toast.makeText(getApplicationContext(), getString(R.string.mess_nopermission_location), Toast.LENGTH_LONG).show();
            // GEOTAGs will be lost, but file can be accessed. So continue the process.
        }
        callFileSelection();
        super.onRequestPermissionsResult(requestCode, permissions, grantResults);
    }

    /**
     * Open dialog for displaying app information.
     * This app uses IJG code.
     * This matter must be displayed.
     */
    public static class AboutFragmentDialog extends DialogFragment {
        @NonNull
        public Dialog onCreateDialog(Bundle savedInstanceState) {
            AlertDialog.Builder builder = new AlertDialog.Builder(requireActivity());
            builder.setTitle(getString(R.string.button_about));
            builder.setMessage(getString(R.string.app_name)+" \n\n"+getString(R.string.main_ijg));
            builder.setPositiveButton("OK", null);
            return builder.create();
        }
    }

    /**
     * When app closed, clear variable.
     */
    @Override
    protected void onDestroy() {
        if( isFinishing() ){
            loadUri = null;
            propertyStr = null;
        }
        super.onDestroy();
    }

}