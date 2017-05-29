package com.tinytangent.droidover6;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.DialogInterface;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.SharedPreferences;
import android.content.SharedPreferences.Editor;
import android.net.VpnService;
import android.support.design.widget.TextInputEditText;
import android.support.v4.content.LocalBroadcastManager;
import android.support.v7.app.AlertDialog;
import android.support.v7.app.AppCompatActivity;
import android.os.Bundle;
import android.preference.PreferenceManager;
import android.text.format.Formatter;
import android.util.Log;
import android.view.View;
import android.widget.Button;
import android.widget.LinearLayout;
import android.widget.TextView;
import android.widget.Toast;

import java.net.InetAddress;
import java.net.NetworkInterface;
import java.util.Collections;
import java.util.List;

import static android.widget.Toast.LENGTH_SHORT;

public class MainActivity extends AppCompatActivity {

    boolean uiModeForVpnStarted = false;
    protected static final int VPN_REQUEST_CODE = 0x100;
    protected Button buttonChangeConnectionState = null;
    protected TextInputEditText inputHostName = null;
    protected TextInputEditText inputPort = null;
    protected TextView textViewOutBytes = null;
    protected TextView textViewInBytes = null;
    protected TextView textViewOutSpeed = null;
    protected TextView textViewInSpeed = null;
    protected TextView textViewLocalIPv6Address = null;
    protected TextView textViewVPNIPv4 = null;
    protected TextView textViewDuration = null;
    protected TextView textViewInPacketCount = null;
    protected TextView textViewOutPacketCount = null;
    protected LinearLayout layoutNetworkStatistics = null;
    protected LinearLayout layoutNetworkSpeed = null;
    protected LinearLayout layoutNetworkPacket = null;
    protected long inBytes = 0;
    protected long outBytes = 0;
    protected long inSpeed = 0;
    protected long outSpeed = 0;
    protected int inPackets = 0;
    protected int outPackets = 0;
    protected int duration = 0;
    protected String vpnIPAddress = "";

    protected static String defaultHostName = "2400:8500:1301:736:a133:130:98:2310";
    protected static String defaultPortText = "5678";

    protected BroadcastReceiver vpnStateReceiver = new BroadcastReceiver() {
        @Override
        public void onReceive(Context context, Intent intent) {
            if (intent.getAction().equals(Over6VpnService.BROADCAST_VPN_STATE)) {
                if (intent.getIntExtra("status_code", -1) == BackendIPC.BACKEND_STATE_DISCONNECTED) {
                    /*AlertDialog.Builder builder = new AlertDialog.Builder(MainActivity.this);

                    builder.setMessage("An error occurred. Disconnected from server")
                            .setTitle("Disconnected")
                            .setNeutralButton("OK", new DialogInterface.OnClickListener() {
                                @Override
                                public void onClick(DialogInterface dialog, int which) {
                                    dialog.dismiss();
                                }
                            });
                    AlertDialog dialog = builder.create();
                    dialog.show();*/
                } else {
                    MainActivity.this.inBytes = intent.getLongExtra("in_bytes", 0);
                    MainActivity.this.outBytes = intent.getLongExtra("out_bytes", 0);
                    MainActivity.this.inSpeed = intent.getLongExtra("in_speed", 0);
                    MainActivity.this.outSpeed = intent.getLongExtra("out_speed", 0);
                    MainActivity.this.inPackets = intent.getIntExtra("in_packets", 0);
                    MainActivity.this.outPackets = intent.getIntExtra("out_packets", 0);
                    MainActivity.this.duration = intent.getIntExtra("duration", 0);
                }
            }
            else if (intent.getAction().equals(Over6VpnService.BROADCAST_VPN_IP)) {
                Log.d("MainActivity", "@Line 90");
                vpnIPAddress = intent.getStringExtra("ip");
            }
            updateGUI();
        }
    };

    protected void updateGUI() {
        textViewLocalIPv6Address.setText("IP address: " + getIPAddress(false));
        textViewVPNIPv4.setText("VPN IPv4: " + vpnIPAddress);
        textViewDuration.setText("Duration: " + duration + "s");
        if (ServiceUtil.isServiceRunning(this, Over6VpnService.class)) {
            uiModeForVpnStarted = true;
            buttonChangeConnectionState.setText("Disconnect");
            inputHostName.setEnabled(false);
            inputPort.setEnabled(false);
            layoutNetworkStatistics.setVisibility(View.VISIBLE);
            layoutNetworkSpeed.setVisibility(View.VISIBLE);
            textViewInBytes.setText("In: " +
                    Formatter.formatShortFileSize(MainActivity.this, inBytes));
            textViewOutBytes.setText("Out: " +
                    Formatter.formatShortFileSize(MainActivity.this, outBytes));
            textViewInSpeed.setText(
                    Formatter.formatShortFileSize(MainActivity.this, inSpeed) + "/s");
            textViewOutSpeed.setText(
                    Formatter.formatShortFileSize(MainActivity.this, outSpeed) + "/s");
            textViewInPacketCount.setText("" + MainActivity.this.inPackets);
            textViewOutPacketCount.setText("" + MainActivity.this.outPackets);
            layoutNetworkPacket.setVisibility(View.VISIBLE);
            textViewVPNIPv4.setVisibility(View.VISIBLE);
            textViewDuration.setVisibility(View.VISIBLE);
        } else {
            uiModeForVpnStarted = false;
            buttonChangeConnectionState.setText("Connect");
            inputHostName.setEnabled(true);
            inputPort.setEnabled(true);
            layoutNetworkStatistics.setVisibility(View.GONE);
            layoutNetworkSpeed.setVisibility(View.GONE);
            layoutNetworkPacket.setVisibility(View.GONE);
            textViewVPNIPv4.setVisibility(View.GONE);
            textViewDuration.setVisibility(View.GONE);
        }
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);
        buttonChangeConnectionState = (Button) findViewById(R.id.button_change_connection_state);
        inputHostName = (TextInputEditText) findViewById(R.id.edit_text_host_name);
        inputPort = (TextInputEditText) findViewById(R.id.edit_text_port);
        textViewOutBytes = (TextView) findViewById(R.id.text_view_out_bytes);
        textViewInBytes = (TextView) findViewById(R.id.text_view_in_bytes);
        textViewOutSpeed = (TextView) findViewById(R.id.text_view_out_speed);
        textViewInSpeed = (TextView) findViewById(R.id.text_view_in_speed);
        textViewLocalIPv6Address = (TextView) findViewById(R.id.text_view_local_ip_v6);
        textViewVPNIPv4 = (TextView) findViewById(R.id.text_view_vpn_ip_v4);
        textViewDuration = (TextView) findViewById(R.id.text_view_duration);
        textViewInPacketCount = (TextView) findViewById(R.id.text_view_in_packet);
        textViewOutPacketCount = (TextView) findViewById(R.id.text_view_out_packet);

        layoutNetworkStatistics = (LinearLayout) findViewById(R.id.linear_layout_network_statistics);
        layoutNetworkSpeed = (LinearLayout) findViewById(R.id.linear_layout_network_speed);
        layoutNetworkPacket = (LinearLayout) findViewById(R.id.linear_layout_network_packet);

        final SharedPreferences settings = PreferenceManager.getDefaultSharedPreferences(this);

        // Set host name & port from defaults
        String hostName = settings.getString("droidOver6_hostName", defaultHostName);
        inputHostName.setText(hostName);
        String portText = settings.getString("droidOver6_portText", defaultPortText);
        inputPort.setText(portText);

        buttonChangeConnectionState.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                // Update defaults
                Editor edit = settings.edit();
                edit.putString("droidOver6_hostName", inputHostName.getText().toString());
                edit.apply();
                edit = settings.edit();
                edit.putString("droidOver6_portText", inputPort.getText().toString());
                edit.apply();

                if (uiModeForVpnStarted) {
                    stopVPNService();
                } else {
                    prepareStartVPN();
                }
            }
        });
        LocalBroadcastManager.getInstance(this).registerReceiver(vpnStateReceiver,
                new IntentFilter(Over6VpnService.BROADCAST_VPN_STATE));
        LocalBroadcastManager.getInstance(this).registerReceiver(vpnStateReceiver,
                new IntentFilter(Over6VpnService.BROADCAST_VPN_IP));
    }


    @Override
    public void onResume() {
        super.onResume();
        updateGUI();
    }

    @Override
    protected void onActivityResult(int requestCode, int resultCode, Intent data) {
        super.onActivityResult(requestCode, resultCode, data);
        if (requestCode == VPN_REQUEST_CODE && resultCode == RESULT_OK) {
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
        if (ServiceUtil.isServiceRunning(this, Over6VpnService.class)) {
            updateGUI();
            return;
        }
        Intent intent = new Intent(this, Over6VpnService.class);
        intent.putExtra("host_name", inputHostName.getText().toString());
        intent.putExtra("port", Integer.parseInt(inputPort.getText().toString()));
        startService(intent);
        inBytes = 0;
        outBytes = 0;
        updateGUI();
    }

    protected void stopVPNService() {
        if (!ServiceUtil.isServiceRunning(this, Over6VpnService.class)) {
            updateGUI();
            return;
        }
        Over6VpnService.getInstance().reliableStop();
        updateGUI();
    }

    public static String getIPAddress(boolean useIPv4) {
        try {
            List<NetworkInterface> interfaces = Collections.list(NetworkInterface.getNetworkInterfaces());
            for (NetworkInterface intf : interfaces) {
                List<InetAddress> addrs = Collections.list(intf.getInetAddresses());
                for (InetAddress addr : addrs) {
                    if (!addr.isLoopbackAddress() && !addr.isLinkLocalAddress()) {
                        String sAddr = addr.getHostAddress();
                        //boolean isIPv4 = InetAddressUtils.isIPv4Address(sAddr);
                        boolean isIPv4 = sAddr.indexOf(':')<0;

                        if (useIPv4) {
                            if (isIPv4)
                                return sAddr;
                        } else {
                            if (!isIPv4) {
                                int delim = sAddr.indexOf('%'); // drop ip6 zone suffix
                                return delim<0 ? sAddr.toUpperCase() : sAddr.substring(0, delim).toUpperCase();
                            }
                        }
                    }
                }
            }
        } catch (Exception ex) { } // for now eat exceptions
        return "";
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
