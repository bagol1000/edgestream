test_that("node_index assigns 0-indexed IDs in insertion order", {
  idx <- node_index()
  expect_equal(node_id(idx, "A"), 0L)
  expect_equal(node_id(idx, "B"), 1L)
  expect_equal(node_id(idx, "A"), 0L)   #same id on repeat call
  expect_equal(get_id(idx, "A"), 0L)
  expect_equal(get_key(idx, 0L), "A")
  expect_equal(n_indexed(idx), 2L)
})

test_that("node_index integrates with a graph", {
  G <- stream_graph()
  idx <- node_index()
  add_edge(G, node_id(idx, "alice"), node_id(idx, "bob"))
  add_edge(G, node_id(idx, "bob"), node_id(idx, "charlie"))
  expect_equal(n_nodes(G), 3L)
  expect_true(same_component(G, get_id(idx, "alice"), get_id(idx, "charlie")))
})

test_that("has_key and missing-key errors behave", {
  idx <- node_index()
  node_id(idx, "x")
  expect_true(has_key(idx, "x"))
  expect_false(has_key(idx, "y"))
  expect_error(get_id(idx, "y"))
})
