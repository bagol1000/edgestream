test_that("weighted graphs store and query weights", {
  W <- stream_graph(weighted = TRUE)
  add_edge(W, 0L, 1L, w = 2.5)
  add_edge(W, 1L, 2L, w = 1.5)
  expect_equal(edge_weight(W, 0L, 1L), 2.5)
  expect_equal(edge_weight(W, 1L, 0L), 2.5)  # undirected symmetry
  expect_equal(strength(W, 1L), 4.0)
  expect_equal(total_weight(W), 4.0)
  expect_false(add_edge(W, 0L, 1L, w = 99))  # duplicate keeps stored weight
  expect_equal(edge_weight(W, 0L, 1L), 2.5)
  expect_error(add_edge(W, 3L, 4L, w = 0), "positive")
  expect_error(edge_weight(W, 0L, 9L), "not found")
})

test_that("weighted batch keeps the first duplicate's weight", {
  W <- stream_graph(weighted = TRUE)
  added <- add_edges(W, c(0L, 0L, 1L), c(1L, 1L, 2L), ws = c(5, 9, 2))
  expect_equal(added, 2)
  expect_equal(edge_weight(W, 0L, 1L), 5)
})

test_that("weighted serialisation round-trips", {
  W <- stream_graph(directed = TRUE, weighted = TRUE)
  add_edge(W, 0L, 1L, w = 2.25)
  p <- tempfile(fileext = ".esg")
  save_graph(W, p)
  H <- load_graph(p)
  expect_equal(edge_weight(H, 0L, 1L), 2.25)
})

test_that("remove_edge updates triangles, degrees and components", {
  G <- stream_graph()
  for (e in list(c(0, 1), c(1, 2), c(0, 2), c(2, 3))) add_edge(G, e[1], e[2])
  expect_equal(triangle_count(G), 1)
  expect_true(remove_edge(G, 0L, 2L))
  expect_false(remove_edge(G, 0L, 2L))
  expect_equal(triangle_count(G), 0)
  expect_equal(n_edges(G), 3)
  expect_equal(degree(G, 0L), 1L)
  # lazy component rebuild
  expect_true(remove_edge(G, 2L, 3L))
  expect_equal(n_components(G), 2)  # {0,1,2} + isolated {3}
  expect_equal(n_nodes(G), 4L)      # node 3 stays touched
})

test_that("clustering coefficients match the formula", {
  G <- stream_graph()
  for (e in list(c(0, 1), c(1, 2), c(0, 2), c(0, 3))) add_edge(G, e[1], e[2])
  expect_equal(clustering_coefficient(G, 1L), 1.0)
  expect_equal(clustering_coefficient(G, 0L), 1 / 3)
  expect_equal(clustering_coefficient(G, 3L), 0.0)
  expect_equal(avg_clustering(G), (1 / 3 + 1 + 1 + 0) / 4)
})

test_that("strongly connected components via Tarjan", {
  D <- stream_graph(directed = TRUE)
  for (e in list(c(0, 1), c(1, 2), c(2, 0), c(2, 3))) add_edge(D, e[1], e[2])
  expect_equal(strong_component_ids(D), c(0L, 0L, 0L, 3L))
  expect_equal(n_strong_components(D), 2L)
  # undirected: SCC == weak components
  U <- stream_graph()
  add_edge(U, 0L, 1L); add_edge(U, 2L, 3L)
  expect_equal(n_strong_components(U), 2L)
})

test_that("pagerank sums to one and ranks hubs higher", {
  G <- stream_graph()
  for (i in 1:4) add_edge(G, 0L, i)
  pr <- pagerank(G)
  expect_equal(sum(pr), 1.0, tolerance = 1e-9)
  expect_gt(pr[1], pr[2])  # node 0 (index 1) above the leaves
})

test_that("edge_list returns each edge once with weights", {
  W <- stream_graph(weighted = TRUE)
  add_edge(W, 3L, 1L, w = 2)
  add_edge(W, 1L, 2L, w = 4)
  el <- edge_list(W)
  el <- el[order(el$u, el$v), ]
  expect_equal(el$u, c(1L, 1L))
  expect_equal(el$v, c(2L, 3L))
  expect_equal(el$w, c(4, 2))
})

test_that("igraph conversions round-trip when igraph is installed", {
  skip_if_not_installed("igraph")
  W <- stream_graph(weighted = TRUE)
  add_edge(W, 0L, 1L, w = 2)
  ig <- as_igraph(W)
  expect_equal(igraph::ecount(ig), 1)
  expect_equal(igraph::E(ig)$weight, 2)
  G2 <- from_igraph(igraph::make_ring(5))
  expect_equal(n_edges(G2), 5)
  expect_equal(n_components(G2), 1L)
})
