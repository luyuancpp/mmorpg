package com.game.gateway.repository;

import com.zaxxer.hikari.HikariDataSource;
import org.junit.jupiter.api.Test;
import org.springframework.beans.factory.annotation.Autowired;
import org.springframework.boot.test.autoconfigure.jdbc.AutoConfigureTestDatabase;
import org.springframework.boot.test.autoconfigure.orm.jpa.DataJpaTest;
import org.springframework.test.context.ActiveProfiles;

import javax.sql.DataSource;
import java.sql.SQLException;

import static org.junit.jupiter.api.Assertions.assertEquals;
import static org.junit.jupiter.api.Assertions.assertInstanceOf;

/**
 * 网关 MySQL 连接池大小的配置钉子(G2b 连接预算)。
 *
 * <p>钉的是:主配置 {@code src/main/resources/application.yaml} 里 {@code spring.datasource.hikari} 的
 * {@code maximum-pool-size: 5}、{@code minimum-idle: 2},经 Spring Boot 属性绑定真的落到了运行时的
 * {@link HikariDataSource} 上。读的是池对象的 getter,不是把 yaml 当文本再解析一遍 —— 键名 Spring 认不认、
 * 值有没有被别的配置源盖掉,只有看池对象才知道。
 *
 * <h2>为什么要钉</h2>
 * 这两个值是 MySQL 连接总账的输入:网关每个副本最多占 maximum-pool-size 条,按副本数 × zone 数计入所有 zone
 * 共用的那台 MySQL(见 application.yaml 该段注释)。K8s 上外挂的 ConfigMap
 * (tools/scripts/k8s_deploy.ps1 New-JavaSvcConfigMapYaml)镜像这两个值,部署契约测试比的是「ConfigMap 与这份
 * yaml 一致」;「这份 yaml 写的值在进程里真的生效」由本用例负责。两头少一头,总账算的就不是集群里实际开的连接数。
 * 改这两个值时,本用例的期望值、连接总账、ConfigMap 镜像要一起改。
 *
 * <h2>接线</h2>
 * 照 {@link ZoneConfigRepositoryWiringTest}。{@code @DataJpaTest} 默认把数据源换成内嵌库,那样 hikari 段根本不
 * 参与,所以用 {@code replace = NONE} 保留自动配置出来的池,并显式给一个 H2 内存库 url(库名独立,不与其它用例
 * 共用)。test profile 的 application-test.yaml 只覆盖 url / driver / 账号,<b>不写</b>
 * {@code spring.datasource.hikari.*},所以这里读到的是主配置的值;日后若在测试配置里加 hikari 段,本用例测到的
 * 就不再是主配置,届时要一并改。
 */
@DataJpaTest(properties = "spring.datasource.url=jdbc:h2:mem:pool_config;MODE=MySQL;DB_CLOSE_DELAY=-1")
@AutoConfigureTestDatabase(replace = AutoConfigureTestDatabase.Replace.NONE)
@ActiveProfiles("test")
class GatewayDataSourcePoolConfigTest {

    @Autowired private DataSource dataSource;

    @Test
    void mainApplicationYamlPinsHikariPool() throws SQLException {
        // 按自动配置,注入的应当就是 HikariDataSource 本体;日后若被代理或装饰器包了一层,走 JDBC 标准的 unwrap
        // 取底层池,免得「包了一层」被误报成「池不是 Hikari」。取不到时原样交给下面的断言报出实际类型。
        DataSource pool = dataSource.isWrapperFor(HikariDataSource.class)
                ? dataSource.unwrap(HikariDataSource.class)
                : dataSource;
        HikariDataSource hikari = assertInstanceOf(HikariDataSource.class, pool,
                "网关连接池必须是 HikariCP:spring.datasource.hikari.* 只对它生效");

        assertEquals(5, hikari.getMaximumPoolSize(),
                "maximum-pool-size 应为 application.yaml 写的 5;不是 5 说明 hikari 段没绑到池上,或改了数值没同步连接总账");
        assertEquals(2, hikari.getMinimumIdle(),
                "minimum-idle 应为 application.yaml 写的 2;不是 2 说明 hikari 段没绑到池上,或改了数值没同步 ConfigMap 镜像");
    }
}
