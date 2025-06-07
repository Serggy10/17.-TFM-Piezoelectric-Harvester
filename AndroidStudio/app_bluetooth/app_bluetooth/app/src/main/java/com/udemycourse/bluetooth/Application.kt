package com.udemycourse.bluetooth

import android.annotation.SuppressLint
import android.app.Application
import android.bluetooth.BluetoothAdapter
import android.content.Intent
import android.util.Log
import com.udemycourse.bluetooth.ui.bleViewModel.BLEViewModel
import dagger.hilt.android.HiltAndroidApp

@HiltAndroidApp
class Application : Application() {
    companion object{
        lateinit var bleViewModel : BLEViewModel
        var BluetoothEnabled = false
    }

    override fun onCreate() {
        super.onCreate()

        BluetoothEnabled()

        while(!BluetoothEnabled){
            BluetoothEnabled()
        }

        bleViewModel = BLEViewModel(this)
    }

    @SuppressLint("MissingPermission")
    fun BluetoothEnabled(){
        // Verificar si Bluetooth está habilitado
        val bluetoothAdapter: BluetoothAdapter? = BluetoothAdapter.getDefaultAdapter()

        if (bluetoothAdapter == null) {
            // El dispositivo no soporta Bluetooth
            Log.e("Bluetooth", "El dispositivo no soporta Bluetooth")
            // Aquí puedes manejar el caso donde no haya soporte para Bluetooth, como notificar al usuario.
        } else {
            // Verifica si el Bluetooth está habilitado
            if (!bluetoothAdapter.isEnabled) {
                // Si Bluetooth no está habilitado, pide al usuario que lo habilite
                Log.d("Bluetooth", "Bluetooth no está habilitado. Solicitar al usuario que lo habilite.")
                val enableBtIntent = Intent(BluetoothAdapter.ACTION_REQUEST_ENABLE)
                enableBtIntent.flags = Intent.FLAG_ACTIVITY_NEW_TASK // Necesario para iniciar actividad desde Application
                startActivity(enableBtIntent)
            } else {
                Log.d("Bluetooth", "Bluetooth ya está habilitado.")
                BluetoothEnabled = true
            }
        }
    }
}