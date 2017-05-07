package com.tinytangent.droidover6;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.net.VpnService;
import android.support.design.widget.TextInputEditText;
import android.support.v4.content.LocalBroadcastManager;
import android.support.v7.app.AppCompatActivity;
import android.os.Bundle;
import android.view.View;
import android.widget.Button;
import android.widget.TextView;
import android.widget.Toast;

import static android.widget.Toast.LENGTH_SHORT;

public class MainActivity extends AppCompatActivity {

    protected static final int VPN_REQUEST_CODE = 0x100;
    protected Button buttonChangeConnectionState = null;
    protected TextInputEditText inputHostName = null;
    protected TextInputEditText inputPort = null;

    protected BroadcastReceiver vpnStateReceiver = new BroadcastReceiver()
    {
        @Override
        public void onReceive(Context context, Intent intent)
        {
            if (BackendWrapperVpnService.BROADCAST_VPN_STATE.equals(intent.getAction()))
            {
                if (intent.getBooleanExtra("running", false))
                    ;
            }
        }
    };

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);
        buttonChangeConnectionState = (Button)findViewById(R.id.button_change_connection_state);
        inputHostName = (TextInputEditText)findViewById(R.id.edit_text_host_name);
        inputPort = (TextInputEditText)findViewById(R.id.edit_text_port);
        buttonChangeConnectionState.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                onButtonStartVPN();
            }
        });
        LocalBroadcastManager.getInstance(this).registerReceiver(vpnStateReceiver,
                new IntentFilter(BackendWrapperVpnService.BROADCAST_VPN_STATE));
    }

    protected void onButtonStartVPN() {
        Toast.makeText(this, stringFromJNI(), LENGTH_SHORT).show();
        Intent vpnIntent = VpnService.prepare(this);
        if (vpnIntent != null)
            startActivityForResult(vpnIntent, VPN_REQUEST_CODE);
        else
            onActivityResult(VPN_REQUEST_CODE, RESULT_OK, null);
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

    protected void startVPNService() {
        Intent intent = new Intent(this, BackendWrapperVpnService.class);
        intent.putExtra("host_name", inputHostName.getText().toString());
        intent.putExtra("port", Integer.parseInt(inputPort.getText().toString()));
        startService(intent);
    }

    protected void onButtonStopVPN() {

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
