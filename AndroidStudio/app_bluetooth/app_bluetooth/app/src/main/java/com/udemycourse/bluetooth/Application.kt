package com.udemycourse.bluetooth

import android.annotation.SuppressLint
import android.app.Application
import android.bluetooth.BluetoothAdapter
import android.content.Intent
import android.util.Log
import dagger.hilt.android.HiltAndroidApp

@HiltAndroidApp
class Application : Application() {
    // No se pone nada de bleViewModel aquí
    var BluetoothEnabled = false

    override fun onCreate() {
        super.onCreate()

        BluetoothEnabled()

        while (!BluetoothEnabled) {
            BluetoothEnabled()
        }
    }

    @SuppressLint("MissingPermission")
    fun BluetoothEnabled() {
        val bluetoothAdapter: BluetoothAdapter? = BluetoothAdapter.getDefaultAdapter()

        if (bluetoothAdapter == null) {
            Log.e("Bluetooth", "El dispositivo no soporta Bluetooth")
        } else {
            if (!bluetoothAdapter.isEnabled) {
                Log.d("Bluetooth", "Bluetooth no está habilitado. Solicitar al usuario que lo habilite.")
                val enableBtIntent = Intent(BluetoothAdapter.ACTION_REQUEST_ENABLE)
                enableBtIntent.flags = Intent.FLAG_ACTIVITY_NEW_TASK
                startActivity(enableBtIntent)
            } else {
                Log.d("Bluetooth", "Bluetooth ya está habilitado.")
                BluetoothEnabled = true
            }
        }
    }
}
