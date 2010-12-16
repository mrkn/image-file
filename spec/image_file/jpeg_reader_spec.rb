require 'spec_helper'

RECOMPILE_CAT_JPG = File.expand_path(File.join('support', 'recompile_cat.jpg'), SPEC_DIR).freeze
RECOMPILE_CAT_CMYK_JPG = File.expand_path(File.join('support', 'recompile_cat_CMYK.jpg'), SPEC_DIR).freeze
RECOMPILE_CAT_PNG = RECOMPILE_CAT_JPG.sub(/\.jpg\Z/, '.png').freeze

module ImageFile
  describe JpegReader do
    context "created by open" do
      subject { described_class.open(RECOMPILE_CAT_JPG) }
      it { should be_source_will_be_closed }
    end
  end

  describe JpegReader, "for 'recompile_cat.jpg'" do #{{{
    subject { described_class.open(RECOMPILE_CAT_JPG) }

    its(:image_width) { should be == 500 }
    its(:image_height) { should be == 300 }
    its(:num_components) { should be == 3 }
    its(:jpeg_color_space) { should be == :YCbCr }

    its(:out_color_space) { should be == :RGB }
    its(:scale) { should be == 1 }
    its(:output_gamma) { should be == 1 }
    it { should_not be_buffered_image }
    its(:dct_method) { should be == :ISLOW }
    it { should_not be_quantize_colors }
    its(:dither_mode) { should be == :FS }

    its(:output_width) { should be == 500 }
    its(:output_height) { should be == 300 }
    its(:output_components) { should be == 3 }

    context "with scale of 1/2" do
      before { subject.scale = 1.quo(2) }
      its(:output_width) { should be == 250 }
      its(:output_height) { should be == 150 }
    end

    context "after setting output_gamma to 1.2" do
      before { subject.output_gamma = 1.2 }
      its(:output_gamma) { should be == 1.2 }
    end

    describe :read_image do
      subject { described_class.open(RECOMPILE_CAT_JPG).read_image }
      its(:width) { should be == 500 }
      its(:height) { should be == 300 }
      its(:row_stride) { should be == 500 }
      its(:pixel_format) { should be == :RGB24 }
    end

    describe :read_image, "with row_stride:42 (< width)" do
      subject { described_class.open(RECOMPILE_CAT_JPG).read_image(row_stride:42) }
      its(:row_stride) { should be == 500 }
    end

    describe :read_image, "with row_stride:512 (> width)" do
      subject { described_class.open(RECOMPILE_CAT_JPG).read_image(row_stride:512) }
      its(:row_stride) { should be == 512 }
    end

    describe :read_image, "with pixel_format: :ARGB32" do
      subject { described_class.open(RECOMPILE_CAT_JPG).read_image(pixel_format: :ARGB32) }
      its(:pixel_format) { should be == :ARGB32 }
    end

    describe :read_image, "with pixel_format: :xyzzy" do
      subject { described_class.open(RECOMPILE_CAT_JPG).read_image(pixel_format: :xyzzy) }
      its(:pixel_format) { should be == :RGB24 }
    end
  end #}}}

  describe JpegReader, "for 'recompile_cat_CMYK.jpg'" do #{{{
    subject { described_class.open(RECOMPILE_CAT_CMYK_JPG) }
    its(:num_components) { should be == 4 }
    its(:jpeg_color_space) { should be == :CMYK }
    its(:out_color_space) { should be == :CMYK }

    describe :read_image do
      subject { described_class.open(RECOMPILE_CAT_CMYK_JPG).read_image }
      its(:width) { should be == 500 }
      its(:height) { should be == 300 }
      its(:row_stride) { should be == 500 }
      its(:pixel_format) { should be == :RGB24 }
    end
  end #}}}

  describe JpegReader, "for 'recompile_cat.png'" do #{{{
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

    describe :read_image do
      it { expect { subject.read_image }.to raise_error(described_class::Error) }
    end
  end #}}}
end

# vim: foldmethod=marker
