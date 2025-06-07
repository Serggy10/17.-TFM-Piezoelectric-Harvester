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
    private val CHAR_TEMP_UUID = UUID.fromString("0000aaa1-0000-1000-8000-00805f9b34fb")
    private val CHAR_HUM_AIR_UUID = UUID.fromString("0000aaa2-0000-1000-8000-00805f9b34fb")
    private val CHAR_HUM_SOIL_UUID = UUID.fromString("0000aaa3-0000-1000-8000-00805f9b34fb")
    private val CHAR_LUX_UUID = UUID.fromString("0000aaa4-0000-1000-8000-00805f9b34fb")
    private val CHAR_BATT_UUID = UUID.fromString("0000aaa5-0000-1000-8000-00805f9b34fb")
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
    private var charTemp: BluetoothGattCharacteristic? = null
    private var charHumAir: BluetoothGattCharacteristic? = null
    private var charHumSoil: BluetoothGattCharacteristic? = null
    private var charLux: BluetoothGattCharacteristic? = null
    private var charBatt: BluetoothGattCharacteristic? = null
    private var ackCharacteristic: BluetoothGattCharacteristic? = null
    private var descriptorWriteStep = 0

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
        }, 5000)
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
                gatt.discoverServices()
            } else if (newState == BluetoothProfile.STATE_DISCONNECTED) {
                gatt.close()
                gattConnection = null
            }
        }

        override fun onServicesDiscovered(gatt: BluetoothGatt, status: Int) {
            val service = gatt.getService(SERVICE_UUID)
            service?.let {
                charTemp = service.getCharacteristic(CHAR_TEMP_UUID)
                charHumAir = service.getCharacteristic(CHAR_HUM_AIR_UUID)
                charHumSoil = service.getCharacteristic(CHAR_HUM_SOIL_UUID)
                charLux = service.getCharacteristic(CHAR_LUX_UUID)
                charBatt = service.getCharacteristic(CHAR_BATT_UUID)
                ackCharacteristic = service.getCharacteristic(CHAR_ACK_UUID)

                descriptorWriteStep = 0
                val descriptorTemp = charTemp?.getDescriptor(UUID.fromString("00002902-0000-1000-8000-00805f9b34fb"))
                descriptorTemp?.let { desc ->
                    Log.d("BLE_CHAIN", "Activando notify TEMP...")
                    charTemp?.let { gatt.setCharacteristicNotification(it, true) }
                    gatt.writeDescriptor(desc.apply {
                        value = BluetoothGattDescriptor.ENABLE_NOTIFICATION_VALUE
                    })
                }
            }
        }

        override fun onDescriptorWrite(gatt: BluetoothGatt, descriptor: BluetoothGattDescriptor, status: Int) {
            if (status == BluetoothGatt.GATT_SUCCESS) {
                descriptorWriteStep++
                when (descriptorWriteStep) {
                    1 -> {
                        val descriptorHumAir = charHumAir?.getDescriptor(UUID.fromString("00002902-0000-1000-8000-00805f9b34fb"))
                        descriptorHumAir?.let { desc ->
                            Log.d("BLE_CHAIN", "Activando notify HUM_AIR...")
                            charHumAir?.let { gatt.setCharacteristicNotification(it, true) }
                            gatt.writeDescriptor(desc.apply {
                                value = BluetoothGattDescriptor.ENABLE_NOTIFICATION_VALUE
                            })
                        }
                    }
                    2 -> {
                        val descriptorHumSoil = charHumSoil?.getDescriptor(UUID.fromString("00002902-0000-1000-8000-00805f9b34fb"))
                        descriptorHumSoil?.let { desc ->
                            Log.d("BLE_CHAIN", "Activando notify HUM_SOIL...")
                            charHumSoil?.let { gatt.setCharacteristicNotification(it, true) }
                            gatt.writeDescriptor(desc.apply {
                                value = BluetoothGattDescriptor.ENABLE_NOTIFICATION_VALUE
                            })
                        }
                    }
                    3 -> {
                        val descriptorLux = charLux?.getDescriptor(UUID.fromString("00002902-0000-1000-8000-00805f9b34fb"))
                        descriptorLux?.let { desc ->
                            Log.d("BLE_CHAIN", "Activando notify LUX...")
                            charLux?.let { gatt.setCharacteristicNotification(it, true) }
                            gatt.writeDescriptor(desc.apply {
                                value = BluetoothGattDescriptor.ENABLE_NOTIFICATION_VALUE
                            })
                        }
                    }
                    4 -> {
                        val descriptorBatt = charBatt?.getDescriptor(UUID.fromString("00002902-0000-1000-8000-00805f9b34fb"))
                        descriptorBatt?.let { desc ->
                            Log.d("BLE_CHAIN", "Activando notify BATT...")
                            charBatt?.let { gatt.setCharacteristicNotification(it, true) }
                            gatt.writeDescriptor(desc.apply {
                                value = BluetoothGattDescriptor.ENABLE_NOTIFICATION_VALUE
                            })
                        }
                    }
                    else -> {
                        Log.d("BLE_CHAIN", "Activación de notifies completa.")
                    }
                }
            } else {
                Log.e("BLE_CHAIN", "Error en onDescriptorWrite, status=$status")
            }
        }
        override fun onCharacteristicChanged(gatt: BluetoothGatt, characteristic: BluetoothGattCharacteristic) {
            val value = characteristic.value
            val buffer = ByteBuffer.wrap(value).order(ByteOrder.LITTLE_ENDIAN)
            val receivedValues = mutableListOf<Float>()

            for (i in 0 until value.size / 4) {
                val v = buffer.float
                receivedValues.add(v)
            }

            when (characteristic.uuid) {
                CHAR_TEMP_UUID -> {
                    receivedValues.forEach { bleViewModel.addTemp(it) }
                    Log.d("BLE_RECEIVED", "TEMP → Recibidos ${receivedValues.size} valores: $receivedValues")
                }
                CHAR_HUM_AIR_UUID -> {
                    receivedValues.forEach { bleViewModel.addHumAir(it) }
                    Log.d("BLE_RECEIVED", "HUM_AIR → Recibidos ${receivedValues.size} valores: $receivedValues")
                }
                CHAR_HUM_SOIL_UUID -> {
                    receivedValues.forEach { bleViewModel.addHumSoil(it) }
                    Log.d("BLE_RECEIVED", "HUM_SOIL → Recibidos ${receivedValues.size} valores: $receivedValues")
                }
                CHAR_LUX_UUID -> {
                    receivedValues.forEach { bleViewModel.addLux(it) }
                    Log.d("BLE_RECEIVED", "LUX → Recibidos ${receivedValues.size} valores: $receivedValues")
                }
                CHAR_BATT_UUID -> {
                    receivedValues.forEach { bleViewModel.addBatt(it) }
                    Log.d("BLE_RECEIVED", "BATT → Recibidos ${receivedValues.size} valores: $receivedValues")
                }
            }

            bleViewModel.lastConnectionTime = System.currentTimeMillis()

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
        binding.tvLastConnection.text = "Última conexión: ${java.text.SimpleDateFormat("HH:mm:ss").format(time)}"
    }
}
