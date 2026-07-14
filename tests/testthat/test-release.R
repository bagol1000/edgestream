test_that("explicit nodes, canonical components and copy work", {
  G <- stream_graph(n_nodes = 100L)
  expect_true(add_node(G, 7L))
  expect_false(add_node(G, 7L))
  expect_equal(add_nodes(G, 10L, 3L), 3)
  expect_equal(nodes(G), c(7L, 10L, 11L, 12L))
  expect_equal(n_ids(G), 100)

  add_edge(G, 5L, 7L)
  add_edge(G, 1L, 5L)
  expect_equal(component_id(G, 7L), 1L)
  H <- copy_graph(G)
  remove_edge(H, 1L, 5L)
  expect_equal(n_edges(G), n_edges(H) + 1)
})

test_that("v3 serialisation preserves isolates and id range", {
  G <- stream_graph(n_nodes = 200L, weighted = TRUE)
  add_node(G, 99L)
  add_edge(G, 5L, 7L, 2.5)
  remove_edge(G, 5L, 7L)
  p <- tempfile(fileext = ".esg")
  save_graph(G, p)
  H <- load_graph(p)
  expect_equal(nodes(H), c(5L, 7L, 99L))
  expect_equal(n_ids(H), 200)
})

test_that("weights and PageRank parameters are validated", {
  G <- stream_graph(weighted = TRUE)
  expect_error(add_edge(G, 0L, 1L, Inf), "finite")
  expect_error(add_edge(G, 0L, 1L, NaN), "finite")
  add_edge(G, 0L, 1L, 2)
  expect_true(update_edge_weight(G, 0L, 1L, 5))
  expect_equal(strength(G, 0L), 5)
  expect_error(pagerank(G, damping = 1), "damping")
  expect_error(pagerank(G, tol = 0), "tol")
  expect_error(pagerank(G, max_iter = 0L), "max_iter")
})

test_that("igraph conversion preserves isolated vertices", {
  skip_if_not_installed("igraph")
  ig <- igraph::make_empty_graph(3)
  G <- from_igraph(ig)
  expect_equal(n_nodes(G), 3L)
  expect_equal(igraph::vcount(as_igraph(G)), 3)

  sparse <- stream_graph(n_nodes = 100L)
  add_node(sparse, 99L)
  roundtrip <- from_igraph(as_igraph(sparse))
  expect_equal(nodes(roundtrip), 99L)
})

test_that("sliding windows refresh and expire edges", {
  W <- sliding_window_graph(10, weighted = TRUE)
  expect_true(window_add_edge(W, 0L, 1L, 0, 2))
  expect_false(window_add_edge(W, 0L, 1L, 8, 5))
  expect_equal(window_advance(W, 11), 0L)
  S <- window_snapshot(W)
  expect_equal(window_advance(W, 19), 1L)
  expect_equal(n_edges(W$graph), 0)
  expect_equal(n_edges(S), 1)
})

test_that("foreign external pointers are rejected", {
  expect_error(degree(new("externalptr"), 0L), "invalid StreamGraph")
})
