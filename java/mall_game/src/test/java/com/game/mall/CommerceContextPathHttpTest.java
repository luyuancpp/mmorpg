package com.game.mall;

import org.springframework.test.context.TestPropertySource;

/** 部署到上下文路径后，网关签名仍不能代替支付适配器签名。复用完整 HTTP 契约。 */
@TestPropertySource(properties = {
        "server.servlet.context-path=/mall",
        "spring.datasource.url=jdbc:h2:mem:commerce_context_path;MODE=MySQL;DB_CLOSE_DELAY=-1;LOCK_TIMEOUT=5000"
})
class CommerceContextPathHttpTest extends CommerceHttpFlowTest { }
