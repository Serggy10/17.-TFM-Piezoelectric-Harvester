package com.udemycourse.bluetooth.ui.bleViewModel

import android.content.SharedPreferences
import androidx.lifecycle.ViewModel
import com.google.gson.Gson
import com.google.gson.reflect.TypeToken
import dagger.hilt.android.lifecycle.HiltViewModel
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import javax.inject.Inject

@HiltViewModel
class BLEViewModel @Inject constructor() : ViewModel() {

    // Listas de datos temporales
    val tempList = mutableListOf<Float>()
    val humAirList = mutableListOf<Float>()
    val humSoilList = mutableListOf<Float>()
    val luxList = mutableListOf<Float>()
    val battList = mutableListOf<Float>()

    var lastConnectionTime: Long = 0L

    private var prefs: SharedPreferences? = null
    private val gson = Gson()

    fun setPreferences(prefs: SharedPreferences) {
        this.prefs = prefs
    }

    fun addTemp(value: Float) {
        tempList.add(value)
        saveList("tempList", tempList)
    }

    fun addHumAir(value: Float) {
        humAirList.add(value)
        saveList("humAirList", humAirList)
    }

    fun addHumSoil(value: Float) {
        humSoilList.add(value)
        saveList("humSoilList", humSoilList)
    }

    fun addLux(value: Float) {
        luxList.add(value)
        saveList("luxList", luxList)
    }

    fun addBatt(value: Float) {
        battList.add(value)
        saveList("battList", battList)
    }

    fun clearData() {
        tempList.clear()
        humAirList.clear()
        humSoilList.clear()
        luxList.clear()
        battList.clear()

        prefs?.edit()?.clear()?.apply()
    }

    fun loadData() {
        tempList.addAll(loadList("tempList", object : TypeToken<MutableList<Float>>() {}))
        humAirList.addAll(loadList("humAirList", object : TypeToken<MutableList<Float>>() {}))
        humSoilList.addAll(loadList("humSoilList", object : TypeToken<MutableList<Float>>() {}))
        luxList.addAll(loadList("luxList", object : TypeToken<MutableList<Float>>() {}))
        battList.addAll(loadList("battList", object : TypeToken<MutableList<Float>>() {}))
    }

    private fun <T> saveList(key: String, list: MutableList<T>) {
        prefs?.edit()?.putString(key, gson.toJson(list))?.apply()
    }

    private fun <T> loadList(key: String, typeToken: TypeToken<MutableList<T>>): MutableList<T> {
        val json = prefs?.getString(key, null)
        return if (json != null) {
            gson.fromJson(json, typeToken.type)
        } else {
            mutableListOf()
        }
    }
}
