package vecdb;

/**
 * Open-time configuration. The builder defaults to the library defaults:
 * dimension learned from the first stored vector, {@link Metric#L2},
 * {@link Index#FLAT}, HNSW {@code M=16} / {@code efConstruction=100} /
 * {@code efSearch=100}, seed 42.
 *
 * <p>The metric and index type are persisted in the file and win over the
 * values supplied here when opening an existing database.
 *
 * <pre>{@code
 * Options opts = Options.builder()
 *         .dim(384)
 *         .metric(Metric.COSINE)
 *         .index(Index.HNSW)
 *         .build();
 * }</pre>
 */
public final class Options {

    /** Vector length; {@code 0} learns the dimension from the first put. */
    public final long dim;
    /** Distance metric. */
    public final Metric metric;
    /** Index structure. */
    public final Index index;
    /** HNSW {@code M}; {@code 0} uses the library default (16). */
    public final long hnswM;
    /** HNSW build breadth; {@code 0} uses the library default (100). */
    public final long hnswEfConstruction;
    /** HNSW query breadth; {@code 0} uses the library default (100). */
    public final long hnswEfSearch;
    /** RNG seed for HNSW; {@code 0} uses the library default (42). */
    public final long seed;

    private Options(Builder b) {
        this.dim = b.dim;
        this.metric = b.metric;
        this.index = b.index;
        this.hnswM = b.hnswM;
        this.hnswEfConstruction = b.hnswEfConstruction;
        this.hnswEfSearch = b.hnswEfSearch;
        this.seed = b.seed;
    }

    /** The library defaults, read from {@code vecdb_options_default}. */
    public static Options defaults() {
        return builder().build();
    }

    /** A builder pre-filled with the library defaults. */
    public static Builder builder() {
        return new Builder();
    }

    @Override
    public String toString() {
        return "Options(dim=" + dim + ", metric=" + metric + ", index=" + index
                + ", hnswM=" + hnswM + ", hnswEfConstruction=" + hnswEfConstruction
                + ", hnswEfSearch=" + hnswEfSearch + ", seed=" + seed + ")";
    }

    /** Fluent builder for {@link Options}. */
    public static final class Builder {
        private long dim;
        private Metric metric = Metric.L2;
        private Index index = Index.FLAT;
        private long hnswM;
        private long hnswEfConstruction;
        private long hnswEfSearch;
        private long seed;

        Builder() {
            // seed with the library defaults
            Native.OptionsDefaults defaults = Native.optionsDefaults();
            this.dim = defaults.dim;
            this.metric = Metric.of(defaults.metric);
            this.index = Index.of(defaults.index);
            this.hnswM = defaults.hnswM;
            this.hnswEfConstruction = defaults.hnswEfConstruction;
            this.hnswEfSearch = defaults.hnswEfSearch;
            this.seed = defaults.seed;
        }

        public Builder dim(long dim) {
            if (dim < 0) {
                throw new IllegalArgumentException("dim must be non-negative");
            }
            this.dim = dim;
            return this;
        }

        public Builder metric(Metric metric) {
            this.metric = metric == null ? Metric.L2 : metric;
            return this;
        }

        /** Accepts {@code "l2"}, {@code "ip"} or {@code "cosine"}. */
        public Builder metric(String metric) {
            this.metric = Metric.parse(metric);
            return this;
        }

        public Builder index(Index index) {
            this.index = index == null ? Index.FLAT : index;
            return this;
        }

        /** Accepts {@code "flat"} or {@code "hnsw"}. */
        public Builder index(String index) {
            this.index = Index.parse(index);
            return this;
        }

        public Builder hnswM(long hnswM) {
            this.hnswM = hnswM;
            return this;
        }

        public Builder hnswEfConstruction(long hnswEfConstruction) {
            this.hnswEfConstruction = hnswEfConstruction;
            return this;
        }

        public Builder hnswEfSearch(long hnswEfSearch) {
            this.hnswEfSearch = hnswEfSearch;
            return this;
        }

        public Builder seed(long seed) {
            this.seed = seed;
            return this;
        }

        public Options build() {
            return new Options(this);
        }
    }
}
