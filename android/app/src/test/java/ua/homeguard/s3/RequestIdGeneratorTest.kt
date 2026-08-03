package ua.homeguard.s3
import org.junit.Assert.assertNotEquals
import org.junit.Test
import ua.homeguard.s3.network.RequestIdGenerator
class RequestIdGeneratorTest { @Test fun idsAreUnique(){val g=RequestIdGenerator(1);assertNotEquals(g.next(),g.next())} }
