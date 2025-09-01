
#include <gtest/gtest.h>
#include "base/core/utils/hash/consistent_hash_node.h"  // Assuming this is the header file for ConsistentHashNode

// Test fixture for ConsistentHashNode
class ConsistentHashNodeTest : public ::testing::Test {
protected:
	void SetUp() override {
		// Optional: Setup any initial conditions before each test
	}

	void TearDown() override {
		// Optional: Clean up after each test
	}

	// Declare any member variables or helper functions you might need
};

// Test adding nodes
TEST_F(ConsistentHashNodeTest, AddNode) {
	ConsistentHashNode<uint64_t, std::string> node;
	node.add(1, "Node1");
	node.add(2, "Node2");

	EXPECT_EQ(node.size(), 2);
	EXPECT_FALSE(node.empty());
}

// Test removing nodes
TEST_F(ConsistentHashNodeTest, RemoveNode) {
	ConsistentHashNode<uint64_t, std::string> node;
	node.add(1, "Node1");
	node.add(2, "Node2");

	node.remove(1);
	EXPECT_EQ(node.size(), 1);
	EXPECT_FALSE(node.empty());

	auto it = node.GetNodeValue(1);
	EXPECT_EQ(it, node.end());
}

// Test retrieving nodes by hash
TEST_F(ConsistentHashNodeTest, GetNodeByHash) {
	ConsistentHashNode<uint64_t, std::string> node;
	node.add(1, "Node1");
	node.add(2, "Node2");

	auto it = node.GetByHash(1);
	EXPECT_NE(it, node.end());
	EXPECT_EQ(it->second, "Node1");

	it = node.GetByHash(3); // Assuming 3 hashes to a valid key
	EXPECT_NE(it, node.end());
	EXPECT_EQ(it->second, "Node2");
}

// Test edge cases
TEST_F(ConsistentHashNodeTest, EdgeCases) {
	ConsistentHashNode<uint64_t, std::string> node;

	auto it = node.GetByHash(1);
	EXPECT_EQ(it, node.end());

	node.remove(1); // Removing from an empty node should not crash
	EXPECT_TRUE(node.empty());
}

int main(int argc, char** argv) {
	testing::InitGoogleTest(&argc, argv);
	return RUN_ALL_TESTS();
}