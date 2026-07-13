test_that("save_graph / load_graph round-trips graph state", {
  G <- stream_graph()
  for (i in 0:49) add_edge(G, i %% 10, (i * 7 + 3) %% 10)

  path <- tempfile(fileext = ".esg")
  save_graph(G, path)
  H <- load_graph(path)

  expect_equal(n_edges(G), n_edges(H))
  expect_equal(triangle_count(G), triangle_count(H))
  expect_equal(n_components(G), n_components(H))
})

test_that("loaded graph preserves connectivity", {
  set.seed(1)
  G <- stream_graph(n_nodes = 200L)
  for (i in 1:2000) add_edge(G, sample(0:199, 1), sample(0:199, 1))

  path <- tempfile(fileext = ".esg")
  save_graph(G, path)
  H <- load_graph(path)

  same <- vapply(0:199, function(i)
    same_component(G, 0L, i) == same_component(H, 0L, i), logical(1))
  expect_true(all(same))
})
