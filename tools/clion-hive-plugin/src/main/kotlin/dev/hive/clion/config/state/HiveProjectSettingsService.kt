package dev.hive.clion.config.state

import com.intellij.openapi.components.PersistentStateComponent
import com.intellij.openapi.components.Service
import com.intellij.openapi.components.State
import com.intellij.openapi.components.Storage
import com.intellij.openapi.project.Project
import dev.hive.clion.config.model.FeatureDefinition
import dev.hive.clion.config.model.HiveFeaturesFile
import dev.hive.clion.config.model.HiveUiState
import dev.hive.clion.config.model.ModeConfiguration

@Service(Service.Level.PROJECT)
@State(name = "HiveEngineConfig", storages = [Storage("hiveEngineConfig.xml")])
class HiveProjectSettingsService : PersistentStateComponent<HiveProjectSettingsState> {
    private var state = HiveProjectSettingsState()

    override fun getState(): HiveProjectSettingsState = state

    override fun loadState(state: HiveProjectSettingsState) {
        this.state = state
    }

    fun loadUiState(catalog: HiveFeaturesFile): HiveUiState {
        val modes = linkedMapOf<String, ModeConfiguration>()
        val featuresById = catalog.categories.flatMap { it.features }.associateBy(FeatureDefinition::id)

        for (mode in catalog.engineModes) {
            val presetState = state.presets[mode]
                ?: legacyPresetFor(mode)
                ?: HiveFeaturePresetState()
            val featureValues = linkedMapOf<String, String>()
            for ((featureId, feature) in featuresById) {
                featureValues[featureId] = presetState.features[featureId] ?: feature.defaultValueFor(mode)
            }
            val buildConfig = state.buildConfigs[mode]
                ?: catalog.presetDefaults[mode]?.buildType
                ?: catalog.buildConfigs.firstOrNull()
                ?: "Debug"
            modes[mode] = ModeConfiguration(buildConfig, featureValues)
        }

        val selectedMode = if (state.engineMode in modes) state.engineMode else modes.keys.firstOrNull().orEmpty()
        return HiveUiState(selectedMode, modes)
    }

    fun saveUiState(uiState: HiveUiState) {
        state.engineMode = uiState.selectedMode
        state.buildConfigs = linkedMapOf<String, String>().apply {
            uiState.modes.forEach { (mode, config) -> put(mode, config.buildConfig) }
        }
        state.presets = linkedMapOf<String, HiveFeaturePresetState>().apply {
            uiState.modes.forEach { (mode, config) ->
                put(mode, HiveFeaturePresetState().also { it.features = LinkedHashMap(config.features) })
            }
        }

        state.editorPreset = state.presets["Editor"]
        state.gamePreset = state.presets["Game"]
        state.headlessPreset = state.presets["Headless"]

        val updatedCustomPresets = LinkedHashMap(state.customPresets)
        state.headlessPreset?.let { updatedCustomPresets["HEADLESS"] = it }
        state.customPresets = updatedCustomPresets
    }

    private fun legacyPresetFor(mode: String): HiveFeaturePresetState? =
        when (mode) {
            "Editor" -> state.editorPreset
            "Game" -> state.gamePreset
            "Headless" -> state.headlessPreset ?: state.customPresets["HEADLESS"] ?: state.customPresets["Headless"]
            else -> state.customPresets[mode]
        }

    companion object {
        fun getInstance(project: Project): HiveProjectSettingsService =
            project.getService(HiveProjectSettingsService::class.java)
    }
}

class HiveProjectSettingsState {
    var engineMode: String = "Editor"
    var buildConfigs: MutableMap<String, String> = linkedMapOf()
    var presets: MutableMap<String, HiveFeaturePresetState> = linkedMapOf()
    var editorPreset: HiveFeaturePresetState? = null
    var gamePreset: HiveFeaturePresetState? = null
    var headlessPreset: HiveFeaturePresetState? = null
    var customPresets: MutableMap<String, HiveFeaturePresetState> = linkedMapOf()
}

class HiveFeaturePresetState {
    var features: MutableMap<String, String> = linkedMapOf()
}
