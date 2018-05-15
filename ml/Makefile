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
