package dev.hive.clion.config.toolchain

import com.intellij.openapi.application.ApplicationManager
import com.intellij.openapi.diagnostic.thisLogger
import com.intellij.openapi.project.Project
import com.intellij.openapi.util.SystemInfo
import com.intellij.openapi.vfs.LocalFileSystem
import dev.hive.clion.config.model.CompilerFamily
import dev.hive.clion.config.model.ToolchainPlatform
import dev.hive.clion.config.model.ToolchainSnapshot
import java.nio.charset.StandardCharsets

object ToolchainDetection {
    fun detect(project: Project): ToolchainSnapshot {
        detectFromClionToolchain()?.let { return it }
        detectFromExistingCaches(project)?.let { return it }
        return ToolchainSnapshot(
            platform = hostPlatform(),
            compilerFamily = CompilerFamily.UNKNOWN,
            toolchainName = "Host defaults",
            detectionSource = "host",
        )
    }

    private fun detectFromClionToolchain(): ToolchainSnapshot? {
        return runCatching {
            val toolchainsClass = Class.forName("com.jetbrains.cidr.cpp.toolchains.CPPToolchains")
            val instance = toolchainsClass.getMethod("getInstance").invoke(null)
            val toolchain = toolchainsClass.methods
                .firstOrNull { it.name == "getDefaultToolchain" && it.parameterCount == 0 }
                ?.invoke(instance)
                ?: return null

            val strings = linkedSetOf<String>()
            collectStrings(toolchain, strings)
            invoke(toolchain, "getEnvironment")?.let { collectStrings(it, strings) }
            invoke(toolchain, "getExecutionEnvironment")?.let { collectStrings(it, strings) }
            invoke(toolchain, "getCppEnvironment")?.let { collectStrings(it, strings) }

            val aggregate = strings.joinToString(" | ").lowercase()
            val platform = detectPlatform(aggregate, hostPlatform())
            val compilerFamily = detectCompilerFamily(aggregate)
            val name = invokeString(toolchain, "getName")
                ?: invokeString(toolchain, "getDisplayName")
                ?: strings.firstOrNull()
                ?: "CLion default toolchain"

            ToolchainSnapshot(platform, compilerFamily, name, "clion")
        }.onFailure {
            thisLogger().warn("Unable to inspect CLion toolchain", it)
        }.getOrNull()
    }

    private fun detectFromExistingCaches(project: Project): ToolchainSnapshot? {
        val candidates = listOf(
            "out/build/hive-editor/CMakeCache.txt",
            "out/build/hive-game/CMakeCache.txt",
            "out/build/hive-headless/CMakeCache.txt",
        )

        for (relativePath in candidates) {
            val file = LocalFileSystem.getInstance().findFileByPath("${project.basePath}/$relativePath") ?: continue
            val content = ApplicationManager.getApplication().runReadAction<String> {
                String(file.contentsToByteArray(), StandardCharsets.UTF_8)
            }
            val compilerLine = content.lineSequence().firstOrNull { it.startsWith("CMAKE_CXX_COMPILER:FILEPATH=") }
            val systemLine = content.lineSequence().firstOrNull { it.startsWith("CMAKE_SYSTEM_NAME:") }
            val aggregate = listOfNotNull(compilerLine, systemLine).joinToString(" | ").lowercase()
            return ToolchainSnapshot(
                platform = detectPlatform(aggregate, hostPlatform()),
                compilerFamily = detectCompilerFamily(aggregate),
                toolchainName = compilerLine?.substringAfter('=') ?: relativePath,
                detectionSource = "cmake-cache",
            )
        }

        return null
    }

    private fun detectPlatform(text: String, fallback: ToolchainPlatform): ToolchainPlatform =
        when {
            "wsl" in text || "linux" in text -> ToolchainPlatform.LINUX
            "mingw" in text || "windows" in text || "visual studio" in text || "msvc" in text -> ToolchainPlatform.WINDOWS
            "darwin" in text || "macos" in text || "apple" in text -> ToolchainPlatform.MACOS
            else -> fallback
        }

    private fun detectCompilerFamily(text: String): CompilerFamily =
        when {
            "clang-cl" in text || "clang++" in text || "llvm" in text || "clang" in text -> CompilerFamily.LLVM
            "g++" in text || "gcc" in text -> CompilerFamily.GCC
            "msvc" in text || "cl.exe" in text || "visual studio" in text -> CompilerFamily.MSVC
            else -> CompilerFamily.UNKNOWN
        }

    private fun collectStrings(instance: Any, sink: MutableSet<String>) {
        sink += instance.javaClass.name
        sink += instance.toString()
        listOf(
            "getName",
            "getDisplayName",
            "getToolchainName",
            "getCompiler",
            "getCCompiler",
            "getCxxCompiler",
            "getCppCompiler",
            "getKind",
            "getCompilerKind",
            "getCCompilerKind",
            "getCppCompilerKind",
        ).forEach { methodName ->
            invokeString(instance, methodName)?.let { sink += it }
        }
    }

    private fun invoke(target: Any, methodName: String): Any? =
        runCatching {
            target.javaClass.methods.firstOrNull { it.name == methodName && it.parameterCount == 0 }?.invoke(target)
        }.getOrNull()

    private fun invokeString(target: Any, methodName: String): String? =
        when (val value = invoke(target, methodName)) {
            null -> null
            is String -> value
            else -> value.toString()
        }

    private fun hostPlatform(): ToolchainPlatform =
        when {
            SystemInfo.isWindows -> ToolchainPlatform.WINDOWS
            SystemInfo.isLinux -> ToolchainPlatform.LINUX
            SystemInfo.isMac -> ToolchainPlatform.MACOS
            else -> ToolchainPlatform.UNKNOWN
        }
}
