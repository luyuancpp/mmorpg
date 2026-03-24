# ECS Design Rules (inspired by Overwatch)

The ECS framework follows these rules: systems are stateless; components have no functions; systems must not call each other directly (shared utility functions are allowed); system iteration is done via "primary component + its sibling components"; singleton components are permitted.