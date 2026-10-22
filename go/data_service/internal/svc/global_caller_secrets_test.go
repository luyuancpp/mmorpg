package svc

import (
	"github.com/stretchr/testify/require"
	"strings"
	"testing"
)

func TestGlobalCallerSecretsAreRequiredAndIndependent(t *testing.T) {
	values := map[string]string{
		"MMORPG_ACCOUNT_DIRECTORY_SECRET_LOGIN":         strings.Repeat("l", 32),
		"MMORPG_ACCOUNT_DIRECTORY_SECRET_TRADE":         strings.Repeat("t", 32),
		"MMORPG_ACCOUNT_DIRECTORY_SECRET_SCENE":         strings.Repeat("s", 32),
		"MMORPG_ACCOUNT_DIRECTORY_SECRET_SCENE_MANAGER": strings.Repeat("r", 32),
	}
	get := func(name string) string { return values[name] }
	require.NoError(t, validateGlobalCallerSecrets(get))
	values["MMORPG_ACCOUNT_DIRECTORY_SECRET_SCENE_MANAGER"] = " " + values["MMORPG_ACCOUNT_DIRECTORY_SECRET_LOGIN"] + " "
	require.Error(t, validateGlobalCallerSecrets(get), "只读SM密钥不能拥有login写能力")
	values["MMORPG_ACCOUNT_DIRECTORY_SECRET_SCENE_MANAGER"] = ""
	require.Error(t, validateGlobalCallerSecrets(get))
}
