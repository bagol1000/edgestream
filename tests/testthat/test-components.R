test_that("union-find tracks components (Model A, touched nodes only)", {
  G <- stream_graph()
  add_edge(G, 0L, 1L)
  add_edge(G, 2L, 3L)
  expect_equal(n_components(G), 2L)
  expect_true(same_component(G, 0L, 1L))
  expect_false(same_component(G, 0L, 2L))
  add_edge(G, 1L, 2L)
  expect_true(same_component(G, 0L, 3L))
  expect_equal(n_components(G), 1L)
})

test_that("duplicates and self-loops are rejected", {
  G <- stream_graph()
  expect_true(add_edge(G, 0L, 1L))
  expect_false(add_edge(G, 0L, 1L))
  expect_false(add_edge(G, 1L, 0L)) # undirected
  expect_false(add_edge(G, 5L, 5L)) # self-loop
  expect_equal(n_edges(G), 1)
})

test_that("component_size and component_nodes agree", {
  G <- stream_graph()
  for (i in 0:8) add_edge(G, i, i + 1L)
  expect_equal(component_size(G, 0L), 10L)
  expect_equal(sort(component_nodes(G, 0L)), 0:9)
})


test_that("component queries for unknown nodes are safe", {
  G <- stream_graph()
  expect_false(same_component(G, 0L, 9999999L))
  expect_equal(component_size(G, 9999999L), 0L)
  expect_error(component_id(G, 999L))
})

test_that("directed graphs keep reverse edges distinct", {
  G <- stream_graph(directed = TRUE)
  expect_true(add_edge(G, 0L, 1L))
  expect_true(add_edge(G, 1L, 0L))
  expect_equal(n_edges(G), 2)
  expect_true(has_edge(G, 0L, 1L))
  expect_true(has_edge(G, 1L, 0L))
  expect_equal(neighbours(G, 0L), 1L)
  expect_equal(neighbours(G, 1L), 0L)
})
