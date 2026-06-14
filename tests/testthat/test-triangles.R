test_that("open path has no triangle, closing one creates it", {
  G <- stream_graph()
  add_edge(G, 0L, 1L)
  add_edge(G, 1L, 2L)
  expect_equal(triangle_count(G), 0)
  add_edge(G, 0L, 2L)
  expect_equal(triangle_count(G), 1)
  expect_equal(local_triangles(G, 0L), 1L)
  expect_equal(local_triangles(G, 1L), 1L)
  expect_equal(local_triangles(G, 2L), 1L)
})

test_that("K4 has 4 triangles, each node in 3", {
  G <- stream_graph()
  for (e in list(c(0,1),c(0,2),c(0,3),c(1,2),c(1,3),c(2,3)))
    add_edge(G, e[1], e[2])
  expect_equal(triangle_count(G), 4)
  expect_equal(local_triangles(G, 0L), 3L)
})

test_that("K10 has C(10,3)=120 triangles, each node in C(9,2)=36", {
  G <- stream_graph()
  for (u in 0:9) for (v in (u + 1):9) if (v <= 9) add_edge(G, u, v)
  expect_equal(triangle_count(G), 120)
  expect_true(all(vapply(0:9, function(i) local_triangles(G, i) == 36L, logical(1))))
  expect_equal(sum(all_local_triangles(G)), 3 * triangle_count(G))
})
