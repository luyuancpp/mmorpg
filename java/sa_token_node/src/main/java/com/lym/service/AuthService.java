package com.lym.service;

import cn.dev33.satoken.stp.StpUtil;
import org.springframework.stereotype.Service;

/**
 * @author lym
 * @description
 * @date 2022/8/26 11:30
 **/
@Service
public class AuthService {
    public boolean isLogin() {
        if (StpUtil.isLogin()) {
            System.out.println("id:" + StpUtil.getLoginIdAsInt());
            System.out.println("token: " + StpUtil.getTokenValue());
        } else {
            System.out.println("Not logged in");
        }

        return StpUtil.isLogin();
    }
}
