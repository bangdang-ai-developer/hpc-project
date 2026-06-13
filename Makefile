MPICC ?= mpicc
CFLAGS ?= -O3 -std=c11 -Wall -Wextra -pedantic -march=native
LDFLAGS ?= -lm

TARGET := build/matrix_hpc
SRC := src/matrix_hpc.c

.PHONY: all clean dirs check demo pipeline multinode-pipeline charts report group-info preflight package

all: dirs $(TARGET)

dirs:
	mkdir -p build results outputs data docs/charts

$(TARGET): $(SRC)
	$(MPICC) $(CFLAGS) -o $@ $(SRC) $(LDFLAGS)

clean:
	rm -f $(TARGET)

check:
	bash scripts/check_local_env.sh

demo: all
	bash scripts/demo.sh

pipeline:
	bash scripts/run_local_pipeline.sh

multinode-pipeline:
	bash scripts/run_multinode_pipeline.sh

charts:
	python3 scripts/make_chart_svgs.py

report:
	python3 scripts/create_report_docx.py

group-info:
	bash scripts/prepare_group_info.sh

preflight:
	bash scripts/preflight_submission.sh

package:
	bash scripts/package_submission.sh
