package com.game.mall;

import org.springframework.test.context.TestPropertySource;

/** 同时配置 context 与 DispatcherServlet 前缀时，支付权限和签名完整路径均须保持。 */
@TestPropertySource(properties = {
        "server.servlet.context-path=/mall",
        "spring.mvc.servlet.path=/commerce",
        "spring.datasource.url=jdbc:h2:mem:commerce_servlet_path;MODE=MySQL;DB_CLOSE_DELAY=-1;LOCK_TIMEOUT=5000"
})
class CommerceServletPathHttpTest extends CommerceHttpFlowTest { }
