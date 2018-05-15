# ML

This directory contains a set of bash scripts and python programs useful for collecting training data, and training ANN models. Training and test data can be generated synthetically or collected via web-scraping google images. Image data is then augmented through multi-cropping of the images.

## Structure

### __ds/__
Directory containing all the datasets for training and model evaluation. The structure is simple and fairly human friendly. The __training__, and __test__ datasets are simply directories contained within __ds__. Labels for each class are represented by partitioning the data into directories __0__, __1__ and __2__ within __training__. Further augmented images are stored within __training/aug__ but follow the same class number labeling scheme.

### __classes/__
This directory contains three plain text files __0__, __1__ and __2__. Each contains google images search queries for returning image thumbnails to use as training data. Each separate query should be listed on its' own line.

<!-- ### __ -->
