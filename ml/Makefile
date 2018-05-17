PY=python3
CLASSES=$(shell ls classes)
EX_PER_QUERY=100

.PHONY: clean
clean:
	rm -f $(addprefix .,$(CLASSES))
	rm -rf ds/training

.%: classes/%
	@for query in $(shell cat $<); do \
		$(PY) img.py subject=$$query count=$(EX_PER_QUERY) >> $@ ;\
	done

ds/training/%: .%
	mkdir -p $@ ;\
	cat $< | ./dl.sh $@ ;\

ds/training/aug/%: ds/training/%
	./all.sh $< | python3 crop.py class=$@

.PHONY: scrape
scrape: $(addprefix ds/training/,$(CLASSES))
	@echo "Downloaded class examples"

.PHONY: augment
augment: $(addprefix ds/training/aug/,$(CLASSES))
	@echo "Augmented class examples"

.PHONY: train
train: augment
	$(PY) trainer.py

.PHONY: help
help:
	@echo "Availible tasks"
	@echo "make clean               Remove all downloaded or synthesized training data"
	@echo "make ds/training/[N]     Download images for class N"
	@echo "make ds/training/aug/[N] Augment images for class N"
	@echo "make scrape              Download images for all specified classes"
	@echo "make augment             Augment all downloaded image classes"
	@echo "make train               Begin training the network with data in ds/training"
