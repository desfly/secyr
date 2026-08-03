package ua.homeguard.s3
import org.junit.Assert.*
import org.junit.Test
import ua.homeguard.s3.model.CommandType
import ua.homeguard.s3.util.isDangerous
class DangerousCommandsTest { @Test fun classification(){assertTrue(CommandType.OPEN_VALVES.isDangerous());assertFalse(CommandType.DISARM.isDangerous())} }
