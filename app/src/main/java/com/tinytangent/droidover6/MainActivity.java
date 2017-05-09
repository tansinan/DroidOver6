package com.tinytangent.droidover6;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.DialogInterface;
import android.content.Intent;
import android.content.IntentFilter;
import android.net.VpnService;
import android.os.Handler;
import android.os.Looper;
import android.support.design.widget.TextInputEditText;
import android.support.v4.content.LocalBroadcastManager;
import android.support.v7.app.AlertDialog;
import android.support.v7.app.AppCompatActivity;
import android.os.Bundle;
import android.view.View;
import android.widget.Button;
import android.widget.Toast;

import static android.widget.Toast.LENGTH_SHORT;

public class MainActivity extends AppCompatActivity {

    boolean uiModeForVpnStarted = false;
    protected static final int VPN_REQUEST_CODE = 0x100;
    protected Button buttonChangeConnectionState = null;
    protected TextInputEditText inputHostName = null;
    protected TextInputEditText inputPort = null;

    protected static String defaultHostName = "2402:f000:5:8701:f400:6eab:6633:5b0a";
    protected static String defaultPortText = "13872";

    protected BroadcastReceiver vpnStateReceiver = new BroadcastReceiver()
    {
        @Override
        public void onReceive(Context context, Intent intent)
        {
            updateGUI();
            if(intent.getAction().equals(Over6VpnService.BROADCAST_VPN_STATE))
            {
                if(intent.getIntExtra("status_code", -1) == BackendIPC.BACKEND_STATE_DISCONNECTED) {
                    AlertDialog.Builder builder = new AlertDialog.Builder(MainActivity.this);

                    builder.setMessage("An error occurred. Disconnected from server")
                            .setTitle("Disconnected")
                            .setNeutralButton("OK", new DialogInterface.OnClickListener() {
                                @Override
                                public void onClick(DialogInterface dialog, int which) {
                                    dialog.dismiss();
                                }
                            });
                    AlertDialog dialog = builder.create();
                    dialog.show();
                }
            }
        }
    };

    protected void updateGUI()
    {
        if(ServiceUtil.isServiceRunning(this, Over6VpnService.class))
        {
            uiModeForVpnStarted = true;
            buttonChangeConnectionState.setText("Disconnect");
            inputHostName.setEnabled(false);
            inputPort.setEnabled(false);
        }
        else
        {
            uiModeForVpnStarted = false;
            buttonChangeConnectionState.setText("Connect");
            inputHostName.setEnabled(true);
            inputPort.setEnabled(true);
        }
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);
        buttonChangeConnectionState = (Button)findViewById(R.id.button_change_connection_state);
        inputHostName = (TextInputEditText)findViewById(R.id.edit_text_host_name);
        inputPort = (TextInputEditText)findViewById(R.id.edit_text_port);
        inputHostName.setText(defaultHostName);
        inputPort.setText(defaultPortText);
        buttonChangeConnectionState.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                if(uiModeForVpnStarted) {
                    stopVPNService();
                }
                else {
                    prepareStartVPN();
                }
            }
        });
        LocalBroadcastManager.getInstance(this).registerReceiver(vpnStateReceiver,
                new IntentFilter(Over6VpnService.BROADCAST_VPN_STATE));
    }


    @Override
    public void onResume()
    {
        super.onResume();
        updateGUI();
    }

    @Override
    protected void onActivityResult(int requestCode, int resultCode, Intent data)
    {
        super.onActivityResult(requestCode, resultCode, data);
        if (requestCode == VPN_REQUEST_CODE && resultCode == RESULT_OK)
        {
            startVPNService();
        }
    }

    protected void prepareStartVPN() {
        Toast.makeText(this, stringFromJNI(), LENGTH_SHORT).show();
        Intent vpnIntent = VpnService.prepare(this);
        if (vpnIntent != null)
            startActivityForResult(vpnIntent, VPN_REQUEST_CODE);
        else
            onActivityResult(VPN_REQUEST_CODE, RESULT_OK, null);
    }

    protected void startVPNService() {
        if(ServiceUtil.isServiceRunning(this, Over6VpnService.class))
        {
            updateGUI();
            return;
        }
        Intent intent = new Intent(this, Over6VpnService.class);
        intent.putExtra("host_name", inputHostName.getText().toString());
        intent.putExtra("port", Integer.parseInt(inputPort.getText().toString()));
        startService(intent);
        updateGUI();
    }

    protected void stopVPNService() {
        if(!ServiceUtil.isServiceRunning(this, Over6VpnService.class))
        {
            updateGUI();
            return;
        }
        Over6VpnService.getInstance().reliableStop();
        updateGUI();
    }

    /**
     * A native method that is implemented by the 'native-lib' native library,
     * which is packaged with this application.
     */
    public native String stringFromJNI();

    // Used to load the 'native-lib' library on application startup.
    static {
        System.loadLibrary("native-lib");
    }
}
