package ua.homeguard.s3.model
data class ComponentHealth(val id:String,val title:String,val state:HealthState,val changedAtMs:Long,val failures:Int)
data class Diagnostics(val overall:HealthState,val activeTransport:Transport,val failedCount:Int,val degradedCount:Int,val components:List<ComponentHealth>,val queuedCommands:Int)
