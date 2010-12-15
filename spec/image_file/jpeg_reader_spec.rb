require 'spec_helper'

RECOMPILE_CAT_JPG = File.expand_path(File.join('support', 'recompile_cat.jpg'), SPEC_DIR).freeze
RECOMPILE_CAT_PNG = RECOMPILE_CAT_JPG.sub(/\.jpg\Z/, '.png').freeze

module ImageFile
  describe JpegReader do
    context "created by open" do
      subject { described_class.open(RECOMPILE_CAT_JPG) }
      it { should be_source_will_be_closed }
    end
  end

  describe JpegReader, "for 'recompile_cat.jpg'" do
    subject { described_class.open(RECOMPILE_CAT_JPG) }

    its(:image_width) { should be == 500 }
    its(:image_height) { should be == 300 }
    its(:num_components) { should be == 3 }
    its(:jpeg_color_space) { should be == :YCbCr }
  end

  describe JpegReader, "for 'recompile_cat.png'" do
    subject { described_class.open(RECOMPILE_CAT_PNG) }

    describe :image_width do
      it { expect { subject.image_width }.to raise_error(described_class::Error) }
    end

    describe :image_height do
      it { expect { subject.image_height }.to raise_error(described_class::Error) }
    end

    describe :num_components do
      it { expect { subject.num_components }.to raise_error(described_class::Error) }
    end

    describe :jpeg_color_space do
      it { expect { subject.jpeg_color_space }.to raise_error(described_class::Error) }
    end
  end
end
