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
import androidx.activity.viewModels  // <-- IMPORTANTE: añadir este import
import androidx.appcompat.app.AppCompatActivity
import androidx.core.app.ActivityCompat
import com.github.mikephil.charting.components.XAxis
import com.github.mikephil.charting.data.*
import com.udemycourse.bluetooth.databinding.ActivityMainBinding
import com.udemycourse.bluetooth.ui.bleViewModel.BLEViewModel
import dagger.hilt.android.AndroidEntryPoint
import java.nio.ByteBuffer
import java.nio.ByteOrder
import java.util.*import android.graphics.Color



@AndroidEntryPoint
class MainActivity : AppCompatActivity() {

    private lateinit var binding: ActivityMainBinding

    // ✅ ViewModel → SOLO esta línea
    private val bleViewModel: BLEViewModel by viewModels()

    private val bluetoothAdapter: BluetoothAdapter by lazy {
        (getSystemService(BLUETOOTH_SERVICE) as BluetoothManager).adapter
    }

    private val bleScanner: BluetoothLeScanner by lazy {
        bluetoothAdapter.bluetoothLeScanner
    }

    private val scanHandler = Handler(Looper.getMainLooper())
    private val scanInterval = 10000L // 30 segundos

    // UUIDs
    private val SERVICE_UUID = UUID.fromString("12345678-1234-1234-1234-1234567890ab")
    private val CHAR_TEMP_UUID = UUID.fromString("12345678-1234-1234-1234-1234567890c1")
    private val CHAR_HUM_AIR_UUID = UUID.fromString("12345678-1234-1234-1234-1234567890c2")
    private val CHAR_HUM_SOIL_UUID = UUID.fromString("12345678-1234-1234-1234-1234567890c3")
    private val CHAR_LUX_UUID = UUID.fromString("12345678-1234-1234-1234-1234567890c4")
    private val CHAR_BATT_UUID = UUID.fromString("12345678-1234-1234-1234-1234567890c5")

    private val requestPermissionLauncher =
        registerForActivityResult(ActivityResultContracts.RequestMultiplePermissions()) { permissions ->
            if (arePermissionsGranted()) {
                startPeriodicScan()
            } else {
                Toast.makeText(this, "Permisos necesarios no concedidos", Toast.LENGTH_SHORT).show()
            }
        }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        binding = ActivityMainBinding.inflate(layoutInflater)
        setContentView(binding.root)
        // ⚙️ Inicializar SharedPreferences y cargar datos
        val sharedPrefs = getSharedPreferences("BLE_DATA", MODE_PRIVATE)
        bleViewModel.setPreferences(sharedPrefs)
        bleViewModel.loadData()

// Actualizar gráficas al arrancar
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
        val bluetoothPermission = ActivityCompat.checkSelfPermission(
            this, BLUETOOTH_CONNECT
        ) == PackageManager.PERMISSION_GRANTED
        val scanPermission = ActivityCompat.checkSelfPermission(
            this, BLUETOOTH_SCAN
        ) == PackageManager.PERMISSION_GRANTED
        val locationPermission = ActivityCompat.checkSelfPermission(
            this, ACCESS_FINE_LOCATION
        ) == PackageManager.PERMISSION_GRANTED

        return bluetoothPermission && scanPermission && locationPermission
    }

    private fun requestPermissions() {
        requestPermissionLauncher.launch(
            arrayOf(
                ACCESS_FINE_LOCATION,
                BLUETOOTH_SCAN,
                BLUETOOTH_CONNECT
            )
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
                .setDeviceName("NODE_01")
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
                gatt.discoverServices()
            } else if (newState == BluetoothProfile.STATE_DISCONNECTED) {
                gatt.close()
            }
        }

        override fun onServicesDiscovered(gatt: BluetoothGatt, status: Int) {
            val service = gatt.getService(SERVICE_UUID)
            service?.let {
                gatt.readCharacteristic(service.getCharacteristic(CHAR_TEMP_UUID))
            }
        }

        override fun onCharacteristicRead(gatt: BluetoothGatt, characteristic: BluetoothGattCharacteristic, status: Int) {
            when (characteristic.uuid) {
                CHAR_TEMP_UUID -> {
                    val temp = ByteBuffer.wrap(characteristic.value).order(ByteOrder.LITTLE_ENDIAN).float
                    bleViewModel.addTemp(temp)
                    gatt.readCharacteristic(gatt.getService(SERVICE_UUID).getCharacteristic(CHAR_HUM_AIR_UUID))
                }
                CHAR_HUM_AIR_UUID -> {
                    val humAir = ByteBuffer.wrap(characteristic.value).order(ByteOrder.LITTLE_ENDIAN).float
                    bleViewModel.addHumAir(humAir)
                    gatt.readCharacteristic(gatt.getService(SERVICE_UUID).getCharacteristic(CHAR_HUM_SOIL_UUID))
                }
                CHAR_HUM_SOIL_UUID -> {
                    val humSoil = ByteBuffer.wrap(characteristic.value).order(ByteOrder.LITTLE_ENDIAN).float
                    bleViewModel.addHumSoil(humSoil)
                    gatt.readCharacteristic(gatt.getService(SERVICE_UUID).getCharacteristic(CHAR_LUX_UUID))
                }
                CHAR_LUX_UUID -> {
                    val lux = ByteBuffer.wrap(characteristic.value).order(ByteOrder.LITTLE_ENDIAN).int
                    bleViewModel.addLux(lux)
                    gatt.readCharacteristic(gatt.getService(SERVICE_UUID).getCharacteristic(CHAR_BATT_UUID))
                }
                CHAR_BATT_UUID -> {
                    val batt = ByteBuffer.wrap(characteristic.value).order(ByteOrder.LITTLE_ENDIAN).float
                    bleViewModel.addBatt(batt)

                    bleViewModel.lastConnectionTime = System.currentTimeMillis()

                    gatt.disconnect()

                    runOnUiThread {
                        updateCharts()
                        updateLastConnection()
                    }
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

        // 🎨 Colores en modo oscuro (o siempre)
        chart.axisLeft.textColor = Color.CYAN
        chart.xAxis.textColor = Color.CYAN
        chart.legend.textColor = Color.CYAN
        chart.axisRight.textColor = Color.CYAN
    }


    private fun updateCharts() {
        binding.chartTemp.data = createLineData(bleViewModel.tempList.mapIndexed { i, v -> Entry(i.toFloat(), v) })
        binding.chartHumAir.data = createLineData(bleViewModel.humAirList.mapIndexed { i, v -> Entry(i.toFloat(), v) })
        binding.chartHumSoil.data = createLineData(bleViewModel.humSoilList.mapIndexed { i, v -> Entry(i.toFloat(), v) })
        binding.chartLux.data = createLineData(bleViewModel.luxList.mapIndexed { i, v -> Entry(i.toFloat(), v.toFloat()) })
        binding.chartBatt.data = createLineData(bleViewModel.battList.mapIndexed { i, v -> Entry(i.toFloat(), v) })

        binding.chartTemp.invalidate()
        binding.chartHumAir.invalidate()
        binding.chartHumSoil.invalidate()
        binding.chartLux.invalidate()
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
