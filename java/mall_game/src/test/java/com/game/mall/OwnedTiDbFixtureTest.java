package com.game.mall;

import static org.junit.jupiter.api.Assertions.*;

import org.junit.jupiter.params.ParameterizedTest;
import org.junit.jupiter.params.provider.NullSource;
import org.junit.jupiter.params.provider.ValueSource;

/** 只解析端点，不连接SQL；真实运行本轮只允许root已核归属的127.0.0.1:24000。 */
class OwnedTiDbFixtureTest {
    @ParameterizedTest
    @ValueSource(strings = {"jdbc:mysql://127.0.0.1:24000", "jdbc:mysql://127.0.0.1:24000/", "jdbc:mysql://[::1]:24000/"})
    void acceptsNumericLoopbackWithExplicitPort(String endpoint) {
        assertTrue(OwnedTiDbFixture.endpoint(endpoint).endsWith("/"));
    }

    @ParameterizedTest
    @NullSource
    @ValueSource(strings = {"", "jdbc:mysql://localhost:24000/", "jdbc:mysql://10.0.0.1:24000/",
        "jdbc:mysql://127.0.0.1:0/", "jdbc:mysql://127.0.0.1:65536/", "jdbc:mysql://127.0.0.1/",
        "jdbc:mysql://127.0.0.1:24000/existing", "jdbc:mysql://127.0.0.1:24000/?tidb_snapshot=x",
        "jdbc:mysql://user:password@127.0.0.1:24000/", "jdbc:mysql://127.0.0.1:24000/#fragment"})
    void rejectsForeignOrImplicitTargets(String endpoint) {
        assertThrows(IllegalArgumentException.class, () -> OwnedTiDbFixture.endpoint(endpoint));
    }
}
