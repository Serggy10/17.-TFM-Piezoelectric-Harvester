package com.udemycourse.bluetooth.ui

import android.Manifest.permission.ACCESS_FINE_LOCATION
import android.Manifest.permission.BLUETOOTH_CONNECT
import android.Manifest.permission.BLUETOOTH_SCAN
import android.content.pm.PackageManager
import android.os.Bundle
import android.util.Log
import android.widget.Button
import androidx.activity.result.contract.ActivityResultContracts
import androidx.appcompat.app.AppCompatActivity
import androidx.core.app.ActivityCompat
import com.udemycourse.bluetooth.databinding.ActivityMainBinding
import dagger.hilt.android.AndroidEntryPoint

@AndroidEntryPoint
class MainActivity : AppCompatActivity() {

    private lateinit var binding : ActivityMainBinding

    private lateinit var btn : Button

    //Definimos el ActivityResultLauncher para pedir permisos
    private val requestPermissionLauncher =
        registerForActivityResult(ActivityResultContracts.RequestMultiplePermissions()) { permissions ->
            if (permissions[ACCESS_FINE_LOCATION] == true &&
                permissions[BLUETOOTH_CONNECT] ==  true &&
                permissions[BLUETOOTH_SCAN] == true) {
                Log.d("Permissions", "Request permissions")
            }
        }

    fun arePermissionsGranted():Boolean{
        val bluetoothPermission = ActivityCompat.checkSelfPermission(
            this,
            BLUETOOTH_CONNECT
        ) == PackageManager.PERMISSION_GRANTED
        val scanPermission = ActivityCompat.checkSelfPermission(
            this,
            BLUETOOTH_SCAN
        ) == PackageManager.PERMISSION_GRANTED
        val locationPermissions = ActivityCompat.checkSelfPermission(
            this,
            ACCESS_FINE_LOCATION
        ) == PackageManager.PERMISSION_GRANTED

        return locationPermissions && scanPermission && bluetoothPermission
    }

    fun requestPermissions(){
        requestPermissionLauncher.launch(
            arrayOf(
                ACCESS_FINE_LOCATION,
                BLUETOOTH_SCAN,
                BLUETOOTH_CONNECT
            )
        )
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        binding = ActivityMainBinding.inflate(layoutInflater)
        setContentView(binding.root)

        if(!arePermissionsGranted()){
            requestPermissions()
        }

        bindingComponents()
        initListeners()

    }



    private fun bindingComponents(){
        btn = binding.btn
    }

    private fun initListeners(){
        btn.setOnClickListener{

        }

    }





}