plugins {
    kotlin("jvm") version "2.1.20"
    id("org.jetbrains.intellij.platform") version "2.5.0"
}

group = "dev.hive"
version = "0.1.0"

repositories {
    mavenCentral()
    intellijPlatform {
        defaultRepositories()
    }
}

dependencies {
    implementation("com.fasterxml.jackson.module:jackson-module-kotlin:2.18.3")
    testImplementation(kotlin("test"))
    testRuntimeOnly("junit:junit:4.13.2")

    intellijPlatform {
        clion("2025.2.3")
        bundledPlugin("com.intellij.clion")
    }
}

kotlin {
    jvmToolchain(21)
}

intellijPlatform {
    buildSearchableOptions = false
    instrumentCode = true
    projectName = project.name
    pluginConfiguration {
        ideaVersion {
            sinceBuild = "252"
        }
    }
}

tasks {
    patchPluginXml {
        sinceBuild = "252"
    }

    runIde {
        jvmArgs("-Xmx2g")
    }

    test {
        useJUnitPlatform()
    }
}
