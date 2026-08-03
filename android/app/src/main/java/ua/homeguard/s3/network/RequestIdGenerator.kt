package ua.homeguard.s3.network
import java.util.concurrent.atomic.AtomicLong
class RequestIdGenerator(seed:Long=System.currentTimeMillis()){private val next=AtomicLong(seed shl 12);fun next():Long=next.incrementAndGet()}
