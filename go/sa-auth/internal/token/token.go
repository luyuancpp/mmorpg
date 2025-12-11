package token

import (
	stputil "github.com/click33/sa-token-go/stputil"
)

// Login：登录并返回 token string（可传 device 表示多端）
func Login(uid uint64, device ...string) (string, error) {
	if len(device) > 0 {
		return stputil.Login(uid, device[0])
	}
	return stputil.Login(uid)
}

// CheckToken：校验 token 并返回 loginId（uid）
func CheckToken(token string) (string, error) {
	// IsLogin requires token param in stputil APIs; some functions will use current context header when integrated in frameworks.
	ok := stputil.IsLogin(token)
	if !ok {
		return "", ErrNotLogin
	}
	id, err := stputil.GetLoginID(token)
	if err != nil {
		return "", err
	}

	return id, ErrInvalidLoginID
}

// Logout：登出某个 uid（可带 device）
func Logout(uid uint64, device ...string) error {
	if len(device) > 0 {
		stputil.Logout(uid, device[0])
	} else {
		stputil.Logout(uid)
	}
	return nil
}
