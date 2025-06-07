package com.udemycourse.bluetooth.ui.bleViewModel

import android.annotation.SuppressLint
import android.app.Application
import android.bluetooth.BluetoothAdapter
import android.bluetooth.BluetoothDevice
import android.bluetooth.BluetoothGatt
import android.bluetooth.BluetoothGattCharacteristic
import android.bluetooth.BluetoothManager
import android.bluetooth.le.BluetoothLeScanner
import android.bluetooth.le.ScanCallback
import android.bluetooth.le.ScanResult
import android.content.Context
import android.os.Handler
import android.os.Looper
import android.util.Log
import androidx.lifecycle.AndroidViewModel
import androidx.lifecycle.LiveData
import androidx.lifecycle.MutableLiveData
import java.util.UUID


class BLEViewModel(application: Application) : AndroidViewModel(application){

    //Configurar caracteristicas
    private val SERVICE_UUID = UUID.fromString("0000180d-0000-1000-8000-00805f9b34fb")  //UUID de chatGPT
    private val CHARACTERISTIC_UUID =UUID.fromString("0000180d-0000-1000-8000-00805f8b34fb")

    private var characteristic : BluetoothGattCharacteristic? = null


    //Variables para la conexion
    private val bluetoothManager =
        application.getSystemService(Context.BLUETOOTH_SERVICE) as BluetoothManager
    private val bluetoothAdapter: BluetoothAdapter = bluetoothManager.adapter
    private val bluetoothLeScanner: BluetoothLeScanner = bluetoothAdapter.bluetoothLeScanner
    private var bluetoothGatt: BluetoothGatt? = null

    private val _isConnected = MutableLiveData<Boolean>(false)
    val isConnected: LiveData<Boolean> get() = _isConnected

    private val _deviceName = MutableLiveData<String>()
    val deviceName: LiveData<String> get() = _deviceName

    private val _isScanning = MutableLiveData<Boolean>()
    val isScanning: LiveData<Boolean> get() = _isScanning

    private val _availableDevices = MutableLiveData<MutableList<BluetoothDevice>>(mutableListOf())
    val availableDevices: MutableLiveData<MutableList<BluetoothDevice>> get() = _availableDevices

    private var scanReady: Boolean = true

}