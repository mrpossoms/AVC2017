# __ML__
![activation map](https://raw.githubusercontent.com/mrpossoms/AVC2017/master/ml/.example.png)

This directory contains a set of bash scripts and python programs useful for collecting training data, and training ANN models. Training and test data can be generated synthetically or collected via web-scraping google images. Image data is then augmented through multi-cropping of the images.

## __Usage__
Most tasks can be performed via the __Makefile__ in this directory.
* `$ make scrape`: Scrapes google images and downloads the images.
* `$ make augment`: Multi crops the already downloaded images.
* `$ make train`: Downloads, augments and begins training a model on the resulting data.
* `$ make clean`: Removes all training data and image urls.
* `$ make .X`: Where X is a class number. See __classes/__ below. Scrapes google images and compiles all the urls into a file .X.
* `$ make ds/training/X`: Downloads all the file urls from file .X into directory __ds/training/X__.
* `$ make ds/training/aug/X`: Augments all the images in __ds/training/X__ by multi cropping them. Then stores the results in __ds/training/aug/X__.

## __Structure__

### __ds/__
Directory containing all the datasets for training and model evaluation. The structure is simple and fairly human friendly. The __training__, and __test__ datasets are simply directories contained within __ds__. Labels for each class are represented by partitioning the data into directories __0__, __1__ and __2__ within __training__. Further augmented images are stored within __training/aug__ but follow the same class number labeling scheme.

### __classes/__
This directory contains three plain text files __0__, __1__ and __2__. Each contains google images search queries for returning image thumbnails to use as training data. Each separate query should be listed on its' own line.

### __synth/__
Contains programs for rendering artificial asphalt and hay bale images. For use as training data.

## __Training Data__
Training data can be generated in a few ways, either by downloading tagged images from the internet, or synthesising data by rendering parametric models. The _Makefile_ in this directory will by default download the images from google.
