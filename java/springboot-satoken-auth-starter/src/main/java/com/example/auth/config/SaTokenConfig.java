package com.example.auth.config;

import cn.dev33.satoken.interceptor.SaInterceptor;
import cn.dev33.satoken.router.SaRouter;
import org.springframework.context.annotation.Bean;
import org.springframework.context.annotation.Configuration;
import org.springframework.web.servlet.config.annotation.InterceptorRegistry;
import org.springframework.web.servlet.config.annotation.WebMvcConfigurer;

@Configuration
public class SaTokenConfig implements WebMvcConfigurer {

    // 注册 Sa-Token 拦截器
    @Override
    public void addInterceptors(InterceptorRegistry registry) {

        registry.addInterceptor(new SaInterceptor(handler -> {
            // SaRouter 是新版的路由匹配配置方式
            SaRouter.match("/**")
                    .notMatch("/auth/**")   // 登录相关接口放行
                    .notMatch("/test")      // 你自己的测试接口放行
                    .check(r -> cn.dev33.satoken.stp.StpUtil.checkLogin());
        })).addPathPatterns("/**");
    }

    // SaToken 全局设置（可选）
    @Bean
    public cn.dev33.satoken.config.SaTokenConfig getSaTokenConfig() {
        cn.dev33.satoken.config.SaTokenConfig config = new cn.dev33.satoken.config.SaTokenConfig();
        config.setTokenName("token");     // token 名称
        config.setTimeout(86400);         // token 过期时间 1 天
        config.setActivityTimeout(3600);  // 1 小时再续签
        config.setIsConcurrent(false);    // 同账号是否允许并发登录
        config.setIsShare(false);         // 是否共用 token
        return config;
    }
}
