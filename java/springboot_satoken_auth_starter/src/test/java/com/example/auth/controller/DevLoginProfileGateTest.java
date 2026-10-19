package com.example.auth.controller;

import com.example.auth.service.SaTokenResponses;
import org.junit.jupiter.api.Test;
import org.springframework.boot.test.context.runner.ApplicationContextRunner;
import org.springframework.data.redis.core.RedisTemplate;
import org.springframework.web.bind.annotation.GetMapping;
import org.springframework.web.bind.annotation.PostMapping;

import java.lang.reflect.Method;
import java.util.Arrays;
import java.util.stream.Stream;

import static org.assertj.core.api.Assertions.assertThat;
import static org.mockito.Mockito.mock;

/**
 * /auth/dev-login 会把任意 account 写进 Go login SaTokenProvider 信任的 Redis 键（报上账号名即登录），
 * 只允许在 dev profile 下存在；回归点是它曾挂在无门禁的 AuthController 上。
 */
class DevLoginProfileGateTest {

    private final ApplicationContextRunner runner = new ApplicationContextRunner()
            .withUserConfiguration(DevLoginController.class)
            .withBean(RedisTemplate.class, () -> mock(RedisTemplate.class))
            .withBean(SaTokenResponses.class, () -> mock(SaTokenResponses.class));

    @Test
    void devLoginControllerIsAbsentWithoutDevProfile() {
        runner.run(ctx -> assertThat(ctx).doesNotHaveBean(DevLoginController.class));
    }

    @Test
    void devLoginControllerIsAbsentUnderOtherProfiles() {
        runner.withInitializer(ctx -> ctx.getEnvironment().setActiveProfiles("prod"))
                .run(ctx -> assertThat(ctx).doesNotHaveBean(DevLoginController.class));
    }

    @Test
    void devLoginControllerIsPresentWithDevProfile() {
        runner.withInitializer(ctx -> ctx.getEnvironment().setActiveProfiles("dev"))
                .run(ctx -> assertThat(ctx).hasSingleBean(DevLoginController.class));
    }

    /** 无门禁的 AuthController 上不得再出现 dev-login / logout 映射。 */
    @Test
    void authControllerExposesNoDevOnlyMapping() {
        for (Method m : AuthController.class.getDeclaredMethods()) {
            Stream<String> paths = Stream.concat(
                    m.isAnnotationPresent(GetMapping.class)
                            ? Stream.concat(Arrays.stream(m.getAnnotation(GetMapping.class).value()),
                            Arrays.stream(m.getAnnotation(GetMapping.class).path()))
                            : Stream.empty(),
                    m.isAnnotationPresent(PostMapping.class)
                            ? Stream.concat(Arrays.stream(m.getAnnotation(PostMapping.class).value()),
                            Arrays.stream(m.getAnnotation(PostMapping.class).path()))
                            : Stream.empty());
            assertThat(paths).noneMatch(p -> p.contains("dev-login") || p.contains("logout"));
        }
    }
}
