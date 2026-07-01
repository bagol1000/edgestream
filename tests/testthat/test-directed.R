test_that("directed triangle count is insertion-order independent", {
  orders <- list(
    list(c(0L, 1L), c(0L, 2L), c(1L, 2L)),
    list(c(0L, 2L), c(1L, 2L), c(0L, 1L)),
    list(c(1L, 2L), c(0L, 1L), c(0L, 2L))
  )
  for (edges in orders) {
    G <- stream_graph(directed = TRUE)
    for (e in edges) add_edge(G, e[1], e[2])
    expect_equal(triangle_count(G), 1)
  }
})

test_that("reciprocal directed edges do not double-count triangles", {
  G <- stream_graph(directed = TRUE)
  for (e in list(c(0L, 1L), c(1L, 2L), c(2L, 0L),
                 c(1L, 0L), c(2L, 1L), c(0L, 2L))) {
    add_edge(G, e[1], e[2])
  }
  expect_equal(n_edges(G), 6)
  expect_equal(triangle_count(G), 1)
})

test_that("directed serialisation round-trips triangle counts", {
  G <- stream_graph(directed = TRUE)
  for (e in list(c(0L, 2L), c(1L, 2L), c(0L, 1L), c(3L, 0L), c(3L, 1L))) {
    add_edge(G, e[1], e[2])
  }
  p <- tempfile(fileext = ".sgph")
  save_graph(G, p)
  H <- load_graph(p)
  expect_equal(n_edges(H), n_edges(G))
  expect_equal(triangle_count(H), triangle_count(G))
  expect_equal(all_local_triangles(H), all_local_triangles(G))
})

test_that("directed betweenness respects edge direction", {
  G <- stream_graph(directed = TRUE)
  add_edge(G, 0L, 1L)
  add_edge(G, 1L, 2L)
  bc <- betweenness_approx(G, k = 1000L, seed = 42L)
  expect_equal(bc, c(0, 1, 0))

  # 0 -> 1 <- 2: nothing routes through node 1
  H <- stream_graph(directed = TRUE)
  add_edge(H, 0L, 1L)
  add_edge(H, 2L, 1L)
  expect_equal(betweenness_approx(H, k = 1000L, seed = 42L), c(0, 0, 0))
})

test_that("normalise = FALSE returns raw betweenness estimates", {
  G <- stream_graph()
  for (i in 0:3) add_edge(G, i, i + 1L)  # path 0-1-2-3-4
  raw <- betweenness_approx(G, k = 10000L, seed = 42L, normalise = FALSE)
  expect_equal(raw, c(0, 3, 4, 3, 0))
})

test_that("in_degree and in_neighbours work in both modes", {
  G <- stream_graph(directed = TRUE)
  add_edge(G, 0L, 2L)
  add_edge(G, 1L, 2L)
  add_edge(G, 2L, 3L)
  expect_equal(degree(G, 2L), 1L)      # out-degree
  expect_equal(in_degree(G, 2L), 2L)
  expect_equal(in_neighbours(G, 2L), c(0L, 1L))
  expect_equal(neighbours(G, 2L), 3L)
  expect_equal(in_degree(G, 99L), 0L)  # never referenced

  U <- stream_graph()
  add_edge(U, 0L, 2L)
  add_edge(U, 1L, 2L)
  expect_equal(in_degree(U, 2L), degree(U, 2L))
  expect_equal(in_neighbours(U, 2L), neighbours(U, 2L))
})

test_that("max_degree is maintained incrementally", {
  G <- stream_graph()
  expect_equal(max_degree(G), 0L)
  for (i in 1:5) {
    add_edge(G, 0L, i)
    expect_equal(max_degree(G), i)
  }
})

test_that("load rejects trailing data", {
  G <- stream_graph()
  add_edge(G, 0L, 1L)
  p <- tempfile(fileext = ".sgph")
  save_graph(G, p)
  con <- file(p, "ab")
  writeBin(as.raw(1:4), con)
  close(con)
  expect_error(load_graph(p), "trailing data")
})
