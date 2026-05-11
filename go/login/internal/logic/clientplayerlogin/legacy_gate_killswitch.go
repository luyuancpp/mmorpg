package clientplayerloginlogic

// shouldRejectLegacyRequest implements the kill-switch decision for the
// legacy "Client → cpp gate (TCP) → ClientPlayerLogin.Login" path.
//
// Rejection rule (the test pins all four cells of this truth table):
//
//   isLegacy=false flagEnabled=any   -> false  (new path always allowed)
//   isLegacy=true  flagEnabled=true  -> false  (legacy path served, default state)
//   isLegacy=true  flagEnabled=false -> true   (kill switch fires, ARCH §12 T+2)
//
// Pulled out as its own function so the test below can verify the
// contract without standing up the full LoginLogic / ServiceContext /
// Redis stack. The Login() handler calls this exactly once per
// request, immediately after the SessionDetails-based legacy
// detection, before any lock acquisition or device-set bookkeeping.
func shouldRejectLegacyRequest(isLegacy bool, flagEnabled bool) bool {
	return isLegacy && !flagEnabled
}
