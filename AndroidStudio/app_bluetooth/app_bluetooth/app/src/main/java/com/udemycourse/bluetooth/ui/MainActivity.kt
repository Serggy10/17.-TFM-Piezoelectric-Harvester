package com.udemycourse.bluetooth.ui

import android.Manifest.permission.ACCESS_FINE_LOCATION
import android.Manifest.permission.BLUETOOTH_CONNECT
import android.Manifest.permission.BLUETOOTH_SCAN
import android.bluetooth.*
import android.bluetooth.le.*
import android.content.pm.PackageManager
import android.os.*
import android.util.Log
import android.widget.Toast
import androidx.activity.result.contract.ActivityResultContracts
import androidx.activity.viewModels
import androidx.appcompat.app.AppCompatActivity
import androidx.core.app.ActivityCompat
import com.github.mikephil.charting.components.XAxis
import com.github.mikephil.charting.data.*
import com.udemycourse.bluetooth.databinding.ActivityMainBinding
import com.udemycourse.bluetooth.ui.bleViewModel.BLEViewModel
import dagger.hilt.android.AndroidEntryPoint
import java.nio.ByteBuffer
import java.nio.ByteOrder
import java.util.*
import android.graphics.Color

@AndroidEntryPoint
class MainActivity : AppCompatActivity() {

    private lateinit var binding: ActivityMainBinding
    private val bleViewModel: BLEViewModel by viewModels()

    private val bluetoothAdapter: BluetoothAdapter by lazy {
        (getSystemService(BLUETOOTH_SERVICE) as BluetoothManager).adapter
    }

    private val bleScanner: BluetoothLeScanner by lazy {
        bluetoothAdapter.bluetoothLeScanner
    }

    private val scanHandler = Handler(Looper.getMainLooper())
    private val scanInterval = 10000L

    private val SERVICE_UUID = UUID.fromString("12345678-1234-1234-1234-1234567890ab")
    private val CHAR_ALL_SENSORS_UUID = UUID.fromString("0000aaaa-0000-1000-8000-00805f9b34fb")
    private val CHAR_ACK_UUID = UUID.fromString("0000aaff-0000-1000-8000-00805f9b34fb")

    private val requestPermissionLauncher =
        registerForActivityResult(ActivityResultContracts.RequestMultiplePermissions()) { permissions ->
            if (arePermissionsGranted()) {
                startPeriodicScan()
            } else {
                Toast.makeText(this, "Permisos necesarios no concedidos", Toast.LENGTH_SHORT).show()
            }
        }

    private var gattConnection: BluetoothGatt? = null
    private var charAllSensors: BluetoothGattCharacteristic? = null
    private var ackCharacteristic: BluetoothGattCharacteristic? = null

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        binding = ActivityMainBinding.inflate(layoutInflater)
        setContentView(binding.root)

        val sharedPrefs = getSharedPreferences("BLE_DATA", MODE_PRIVATE)
        bleViewModel.setPreferences(sharedPrefs)
        bleViewModel.loadData()

        updateCharts()
        updateLastConnection()

        if (!arePermissionsGranted()) {
            requestPermissions()
        } else {
            startPeriodicScan()
        }

        initListeners()
        initCharts()
    }

    private fun arePermissionsGranted(): Boolean {
        val bluetoothPermission = ActivityCompat.checkSelfPermission(this, BLUETOOTH_CONNECT) == PackageManager.PERMISSION_GRANTED
        val scanPermission = ActivityCompat.checkSelfPermission(this, BLUETOOTH_SCAN) == PackageManager.PERMISSION_GRANTED
        val locationPermission = ActivityCompat.checkSelfPermission(this, ACCESS_FINE_LOCATION) == PackageManager.PERMISSION_GRANTED

        return bluetoothPermission && scanPermission && locationPermission
    }

    private fun requestPermissions() {
        requestPermissionLauncher.launch(
            arrayOf(ACCESS_FINE_LOCATION, BLUETOOTH_SCAN, BLUETOOTH_CONNECT)
        )
    }

    private fun startPeriodicScan() {
        scanHandler.post(scanRunnable)
    }

    private val scanRunnable = object : Runnable {
        override fun run() {
            startScan()
            scanHandler.postDelayed(this, scanInterval)
        }
    }

    private fun startScan() {
        val filters = listOf(
            ScanFilter.Builder()
                .setDeviceName("NODE_SENSOR")
                .build()
        )

        val settings = ScanSettings.Builder()
            .setScanMode(ScanSettings.SCAN_MODE_LOW_LATENCY)
            .build()

        bleScanner.startScan(filters, settings, scanCallback)

        scanHandler.postDelayed({
            bleScanner.stopScan(scanCallback)
        }, 10000) //5000
    }

    private val scanCallback = object : ScanCallback() {
        override fun onScanResult(callbackType: Int, result: ScanResult) {
            bleScanner.stopScan(this)
            result.device.connectGatt(this@MainActivity, false, gattCallback)
        }
    }

    private val gattCallback = object : BluetoothGattCallback() {
        override fun onConnectionStateChange(gatt: BluetoothGatt, status: Int, newState: Int) {
            if (newState == BluetoothProfile.STATE_CONNECTED) {
                gattConnection = gatt

                val mtuRequest = 128
                Log.d("BLE_MTU", "Solicitando MTU de $mtuRequest bytes...")
                gatt.requestMtu(mtuRequest)

            } else if (newState == BluetoothProfile.STATE_DISCONNECTED) {
                gatt.close()
                gattConnection = null
            }
        }

        override fun onMtuChanged(gatt: BluetoothGatt, mtu: Int, status: Int) {
            if (status == BluetoothGatt.GATT_SUCCESS) {
                Log.d("BLE_MTU", "MTU cambiado a $mtu → descubriendo servicios...")
                gatt.discoverServices()
            } else {
                Log.e("BLE_MTU", "Error cambiando MTU → status=$status")
            }
        }

        override fun onServicesDiscovered(gatt: BluetoothGatt, status: Int) {
            val service = gatt.getService(SERVICE_UUID)
            service?.let {
                charAllSensors = service.getCharacteristic(CHAR_ALL_SENSORS_UUID)
                ackCharacteristic = service.getCharacteristic(CHAR_ACK_UUID)

                charAllSensors?.let { characteristic ->
                    val descriptor = characteristic.getDescriptor(UUID.fromString("00002902-0000-1000-8000-00805f9b34fb"))
                    descriptor?.let { desc ->
                        Log.d("BLE_CHAIN", "Activando notify ALL_SENSORS...")
                        gatt.setCharacteristicNotification(characteristic, true)
                        gatt.writeDescriptor(desc.apply {
                            value = BluetoothGattDescriptor.ENABLE_NOTIFICATION_VALUE
                        })
                    }
                }
            }
        }

        override fun onCharacteristicChanged(gatt: BluetoothGatt, characteristic: BluetoothGattCharacteristic) {
            if (characteristic.uuid == CHAR_ALL_SENSORS_UUID) {
                val value = characteristic.value
                val buffer = ByteBuffer.wrap(value).order(ByteOrder.LITTLE_ENDIAN)

                val sensorSizeBytes = 5 * 4

                val numRecords = value.size / sensorSizeBytes
                Log.d("BLE_RECEIVED", "ALL_SENSORS → Recibidos $numRecords registros:")

                for (i in 0 until numRecords) {
                    val temp = buffer.float
                    val humAir = buffer.float
                    val humSoil = buffer.float
                    val lux = buffer.float
                    val batt = buffer.float

                    bleViewModel.addTemp(temp)
                    bleViewModel.addHumAir(humAir)
                    bleViewModel.addHumSoil(humSoil)
                    bleViewModel.addLux(lux)
                    bleViewModel.addBatt(batt)

                    Log.d("BLE_RECEIVED", "   #${i+1} → Temp=$temp | HumAir=$humAir | HumSoil=$humSoil | Lux=$lux | Batt=$batt")
                }

                bleViewModel.lastConnectionTime = System.currentTimeMillis()
                bleViewModel.saveLastConnectionTime()

                runOnUiThread {
                    updateCharts()
                    updateLastConnection()
                }

                ackCharacteristic?.let { ackChar ->
                    ackChar.value = "OK".toByteArray(Charsets.UTF_8)
                    val result = gatt.writeCharacteristic(ackChar)
                    Log.d("BLE_ACK", "ACK enviado: $result")
                }
            }
        }
    }

    private fun initListeners() {
        binding.btnScan.setOnClickListener {
            startScan()
        }

        binding.btnClear.setOnClickListener {
            bleViewModel.clearData()
            updateCharts()
        }
    }

    private fun initCharts() {
        setupChart(binding.chartTemp)
        setupChart(binding.chartHumAir)
        setupChart(binding.chartHumSoil)
        setupChart(binding.chartLux)
        setupChart(binding.chartBatt)
    }

    private fun setupChart(chart: com.github.mikephil.charting.charts.LineChart) {
        chart.axisLeft.axisMinimum = 0f
        chart.axisRight.isEnabled = false
        chart.xAxis.position = XAxis.XAxisPosition.BOTTOM
        chart.xAxis.granularity = 1f
        chart.description.isEnabled = false

        chart.axisLeft.textColor = Color.CYAN
        chart.xAxis.textColor = Color.CYAN
        chart.legend.textColor = Color.CYAN
        chart.axisRight.textColor = Color.CYAN
    }

    private fun updateCharts() {
        binding.chartTemp.data = createLineData(bleViewModel.tempList.mapIndexed { i, v -> Entry(i.toFloat(), v) })
        binding.chartTemp.invalidate()

        binding.chartHumAir.data = createLineData(bleViewModel.humAirList.mapIndexed { i, v -> Entry(i.toFloat(), v) })
        binding.chartHumAir.invalidate()

        binding.chartHumSoil.data = createLineData(bleViewModel.humSoilList.mapIndexed { i, v -> Entry(i.toFloat(), v) })
        binding.chartHumSoil.invalidate()

        binding.chartLux.data = createLineData(bleViewModel.luxList.mapIndexed { i, v -> Entry(i.toFloat(), v) })
        binding.chartLux.invalidate()

        binding.chartBatt.data = createLineData(bleViewModel.battList.mapIndexed { i, v -> Entry(i.toFloat(), v) })
        binding.chartBatt.invalidate()
    }

    private fun createLineData(entries: List<Entry>): LineData {
        val dataSet = LineDataSet(entries, "Datos")
        dataSet.setDrawCircles(true)
        dataSet.setDrawValues(false)
        return LineData(dataSet)
    }

    private fun updateLastConnection() {
        val time = bleViewModel.lastConnectionTime
        val format = java.text.SimpleDateFormat("dd/MM/yyyy HH:mm:ss", Locale.getDefault())
        binding.tvLastConnection.text = "Última conexión: ${format.format(time)}"

    }
}
