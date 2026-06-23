test_that("add_edges deduplicates within the batch", {
  G <- stream_graph()
  added <- add_edges(G, c(0L, 0L, 1L, 2L), c(1L, 1L, 2L, 0L))
  # (0,1),(0,1)=dup,(1,2),(2,0)=(0,2) -> 3 distinct new edges
  expect_equal(added, 3)
  expect_equal(n_edges(G), 3)
})

test_that("batch result matches sequential", {
  set.seed(0)
  us <- sample(0:99, 500, replace = TRUE)
  vs <- sample(0:99, 500, replace = TRUE)

  G1 <- stream_graph()
  for (i in seq_along(us)) add_edge(G1, us[i], vs[i])

  G2 <- stream_graph()
  add_edges(G2, us, vs, n_threads = 4L)

  expect_equal(n_edges(G1), n_edges(G2))
  expect_equal(triangle_count(G1), triangle_count(G2))
  expect_equal(n_components(G1), n_components(G2))
})


test_that("directed batch keeps reverse edges distinct", {
  G <- stream_graph(directed = TRUE)
  added <- add_edges(G, c(0L, 1L), c(1L, 0L))
  expect_equal(added, 2)
  expect_equal(n_edges(G), 2)
  expect_equal(neighbours(G, 0L), 1L)
  expect_equal(neighbours(G, 1L), 0L)
})
