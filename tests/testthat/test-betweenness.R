test_that("path graph: middle node beats endpoints", {
  G <- stream_graph()
  for (i in 0:3) add_edge(G, i, i + 1L)
  bc <- betweenness_approx(G, k = 1000L, seed = 42L)
  #R is 1-indexed: node 2 -> bc[3], node 0 -> bc[1], node 4 -> bc[5]
  expect_gt(bc[3], bc[1])
  expect_gt(bc[3], bc[5])
  expect_true(all(bc >= 0 & bc <= 1))
})

test_that("star graph: centre has maximum betweenness", {
  G <- stream_graph()
  for (i in 1:9) add_edge(G, 0L, i)
  bc <- betweenness_approx(G, k = 500L, seed = 42L)
  expect_equal(bc[1], max(bc))   #node 0 -> bc[1]
})

test_that("same seed gives identical results", {
  G <- stream_graph()
  for (i in 0:49) add_edge(G, i %% 10, (i * 7 + 3) %% 10)
  b1 <- betweenness_approx(G, k = 100L, seed = 42L)
  b2 <- betweenness_approx(G, k = 100L, seed = 42L)
  expect_identical(b1, b2)
})
