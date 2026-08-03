package ua.homeguard.s3.util
interface Clock { fun nowMs():Long }
object SystemClock:Clock { override fun nowMs()=System.currentTimeMillis() }
